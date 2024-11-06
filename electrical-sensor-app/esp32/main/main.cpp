#include "DeviceCallbacks.h"
#include "PowerTopologyDelegate.h"
#include "ElectricalPowerMeasurementDelegate.h"
#include <ElectricalControllerMain.h>
#include <app/clusters/energy-preference-server/energy-preference-server.h>
#include "EnergyPreferenceDelegate.h"
#include <inttypes.h>


#include "esp_log.h"
#include <common/CHIPDeviceManager.h>
#include <common/Esp32AppServer.h>
#include <common/Esp32ThreadInit.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "shell_extension/launch.h"
#include "shell_extension/openthread_cli_register.h"
#include <app/server/Dnssd.h>
#include <app/server/OnboardingCodesUtil.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <platform/ESP32/ESP32Utils.h>

#include <app/AttributeAccessInterface.h> 
#include <app/util/attribute-storage.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/CommandHandler.h>  
#include "app-common/zap-generated/attributes/Accessors.h"


#include <cstdlib>
#include <ctime>

#if CONFIG_ENABLE_SNTP_TIME_SYNC
#include <time/TimeSync.h>
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "spi_flash_mmap.h"
#else
#include "esp_spi_flash.h"
#endif

#if CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER
#include <platform/ESP32/ESP32FactoryDataProvider.h>
#endif // CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER

#if CONFIG_ENABLE_PW_RPC
#include "Rpc.h"
#endif

#if CONFIG_ENABLE_ESP32_DEVICE_INFO_PROVIDER
#include <platform/ESP32/ESP32DeviceInfoProvider.h>
#else
#include <DeviceInfoProviderImpl.h>
#endif // CONFIG_ENABLE_ESP32_DEVICE_INFO_PROVIDER

#if CONFIG_SEC_CERT_DAC_PROVIDER
#include <platform/ESP32/ESP32SecureCertDACProvider.h>
#endif

#if CONFIG_ENABLE_ESP_INSIGHTS_TRACE
#include <esp_insights.h>
#include <tracing/esp32_trace/esp32_tracing.h>
#include <tracing/registry.h>
#endif

using namespace ::chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceManager;
using namespace ::chip::DeviceLayer;

using chip::app::DataModel::MakeNullable;

static const char * TAG = "electrical-sensor-app";

static AppDeviceCallbacks EchoCallbacks;
static DeviceCallbacksDelegate sAppDeviceCallbacksDelegate;

chip::app::Clusters::ElectricalPowerMeasurement::ElectricalPowerMeasurementDelegate gElectricalPowerMeasurementDelegate;
chip::app::Clusters::PowerTopology::PowerTopologyDelegate gPowerTopologyDelegate;
chip::app::Clusters::EnergyPreferenceDelegate gEnergyPreferenceDelegate;

#if CONFIG_ENABLE_ESP_INSIGHTS_TRACE
extern const char insights_auth_key_start[] asm("_binary_insights_auth_key_txt_start");
extern const char insights_auth_key_end[] asm("_binary_insights_auth_key_txt_end");
#endif

namespace {
#if CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER
DeviceLayer::ESP32FactoryDataProvider sFactoryDataProvider;
#endif // CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER

#if CONFIG_ENABLE_ESP32_DEVICE_INFO_PROVIDER
DeviceLayer::ESP32DeviceInfoProvider gExampleDeviceInfoProvider;
#else
DeviceLayer::DeviceInfoProviderImpl gExampleDeviceInfoProvider;
#endif // CONFIG_ENABLE_ESP32_DEVICE_INFO_PROVIDER

#if CONFIG_SEC_CERT_DAC_PROVIDER
DeviceLayer::ESP32SecureCertDACProvider gSecureCertDACProvider;
#endif // CONFIG_SEC_CERT_DAC_PROVIDER

chip::Credentials::DeviceAttestationCredentialsProvider * get_dac_provider(void)
{
#if CONFIG_SEC_CERT_DAC_PROVIDER
    return &gSecureCertDACProvider;
#elif CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER
    return &sFactoryDataProvider;
#else // EXAMPLE_DAC_PROVIDER
    return chip::Credentials::Examples::GetExampleDACProvider();
#endif
}

} // namespace

//Intervalo de actualizacion de valores en milisegundos
#define UPDATE_INTERVAL_MS 30000 

#define UPDATE_TASK_STACK_SIZE 4096
#define UPDATE_TASK_PRIORITY   (tskIDLE_PRIORITY + 1)

// Constantes para los modos de operación
constexpr uint8_t MODE_NORMAL    = 0;
constexpr uint8_t MODE_SAVING    = 1;
constexpr uint8_t MODE_INTENSIVE = 2;

//Valores iniciales atributos
constexpr int64_t INITIAL_VOLTAGE        = 230000; // en milivoltios
constexpr int64_t INITIAL_ACTIVE_CURRENT = 5000;   // en miliamperios
constexpr int64_t INITIAL_ACTIVE_POWER   = 0;      // en milivatios

//Limites de atributos
constexpr int64_t MIN_VOLTAGE = 220000; // en milivoltios
constexpr int64_t MAX_VOLTAGE = 240000; // en milivoltios
constexpr int64_t MIN_ACTIVE_CURRENT = 0;     // en miliamperios
constexpr int64_t MAX_ACTIVE_CURRENT = 10000; // en miliamperios

// Porcentaje para calcular el offset del voltaje
constexpr int64_t OFFSET_VOLTAGE_PERCENTAGE    = 1; 

// Porcentaje de máxima variación permitida en voltaje
constexpr int64_t OFFSET_VOLTAGE_MAX_VARIATION = 3; 

//Valores de offset de corriente activa segun modo de operacion
constexpr int64_t OFFSET_CURRENT_MODE_NORMAL    = 50;
constexpr int64_t OFFSET_CURRENT_MODE_SAVING    = 5;
constexpr int64_t OFFSET_CURRENT_MODE_INTENSIVE = 500;


// Función para obtener el modo de operación actual a traves del cluster Energy Preference
uint8_t GetOperationMode()
{
    uint8_t currentEnergyBalance;  
    
    {
        chip::DeviceLayer::StackLock lock; 

        auto status = chip::app::Clusters::EnergyPreference::Attributes::CurrentEnergyBalance::Get(1, &currentEnergyBalance);
        
        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            ESP_LOGE(TAG, "Error al leer el atributo CurrentEnergyBalance: %d", static_cast<int>(status));
            return MODE_NORMAL; 
        }
    } 

    switch (currentEnergyBalance)
    {
    case 0:
        return MODE_NORMAL;
    case 1:
        return MODE_SAVING;
    case 2:
        return MODE_INTENSIVE;
    default:
        ESP_LOGW(TAG, "Nivel de control no reconocido (%d), usando modo normal", currentEnergyBalance);
        return MODE_NORMAL;
    }
}

void SetOperationMode(uint8_t newEnergyBalance)
{
    {
        chip::DeviceLayer::StackLock lock;

        auto status = chip::app::Clusters::EnergyPreference::Attributes::CurrentEnergyBalance::Set(1, newEnergyBalance);

        if (status != chip::Protocols::InteractionModel::Status::Success)
        {
            ESP_LOGE(TAG, "Error al escribir el atributo CurrentEnergyBalance: %d", static_cast<int>(status));
        }
        else
        {
            ESP_LOGI(TAG, "Atributo CurrentEnergyBalance actualizado exitosamente a: %d", newEnergyBalance);
        }
    }
}


// Tarea para actualizar periódicamente los atributos de voltaje, corriente y potencia
void UpdateAttributes(void *pvParameters)
{
    int64_t voltage       = INITIAL_VOLTAGE;
    int64_t activeCurrent = INITIAL_ACTIVE_CURRENT;
    int64_t activePower   = INITIAL_ACTIVE_POWER;

    while (true)
    {
        uint8_t operationMode = GetOperationMode();

        int64_t offsetRangeCurrent;
        switch (operationMode)
        {
        case MODE_SAVING:
            offsetRangeCurrent = OFFSET_CURRENT_MODE_SAVING;
            break;
        case MODE_INTENSIVE:
            offsetRangeCurrent = OFFSET_CURRENT_MODE_INTENSIVE;
            break;
        case MODE_NORMAL:
        default:
            offsetRangeCurrent = OFFSET_CURRENT_MODE_NORMAL;
            break;
        }

        int64_t offsetRangeVoltage = voltage * OFFSET_VOLTAGE_PERCENTAGE / 100;

        int64_t offsetActiveCurrent = (std::rand() % (2 * offsetRangeCurrent + 1)) - offsetRangeCurrent;
        int64_t offsetVoltage       = (std::rand() % (2 * offsetRangeVoltage + 1)) - offsetRangeVoltage;

        activeCurrent = std::clamp(activeCurrent + offsetActiveCurrent, MIN_ACTIVE_CURRENT, MAX_ACTIVE_CURRENT);
        voltage       = std::clamp(voltage + offsetVoltage,
                                   INITIAL_VOLTAGE - (INITIAL_VOLTAGE * OFFSET_VOLTAGE_MAX_VARIATION / 100),
                                   INITIAL_VOLTAGE + (INITIAL_VOLTAGE * OFFSET_VOLTAGE_MAX_VARIATION / 100));
        activePower   = (voltage * activeCurrent) / 1000;

        // Actualizar el voltaje
        DataModel::Nullable<int64_t> voltageValue;
        voltageValue.SetNonNull(voltage);

        ESP_LOGI(TAG, "Updating voltage to %lld mV", voltage);

        {
            chip::DeviceLayer::StackLock lock; 
            CHIP_ERROR err = gElectricalPowerMeasurementDelegate.SetVoltage(voltageValue);
            if (err != CHIP_NO_ERROR)
            {
                ESP_LOGE(TAG, "Failed to update voltage: %s", ErrorStr(err));
            }
            else
            {
                ESP_LOGI(TAG, "voltage successfully updated to %lld mV", voltage);
            }
        }

        // Actualizar la corriente activa
        DataModel::Nullable<int64_t> activeCurrentValue;
        activeCurrentValue.SetNonNull(activeCurrent);

        ESP_LOGI(TAG, "Updating active current to %lld mA", activeCurrent);

        {
            chip::DeviceLayer::StackLock lock; 
            CHIP_ERROR err = gElectricalPowerMeasurementDelegate.SetActiveCurrent(activeCurrentValue);
            if (err != CHIP_NO_ERROR)
            {
                ESP_LOGE(TAG, "Failed to update active current: %s", ErrorStr(err));
            }
            else
            {
                ESP_LOGI(TAG, "Active current successfully updated to %lld mA", activeCurrent);
            }
        }

        // Actualizar la potencia activa
        DataModel::Nullable<int64_t> activePowerValue;
        activePowerValue.SetNonNull(activePower);

        ESP_LOGI(TAG, "Updating active power to %lld mW", activePower);

        {
            chip::DeviceLayer::StackLock lock; 
            CHIP_ERROR err = gElectricalPowerMeasurementDelegate.SetActivePower(activePowerValue);
            if (err != CHIP_NO_ERROR)
            {
                ESP_LOGE(TAG, "Failed to update active power %s", ErrorStr(err));
            }
            else
            {
                ESP_LOGI(TAG, "Active power successfully updated to %lld mW", activePower);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_MS));
    }
}


void ApplicationInit()
{
    ESP_LOGD(TAG, "Electrical Sensor App: ApplicationInit()");

    ElectricalControllerInit();

    // Definir y inicializar el endpoint
    EndpointId endpointId = 1;
    gElectricalPowerMeasurementDelegate.SetEndpointId(endpointId);

    EnergyPreference::SetDelegate(&gEnergyPreferenceDelegate);


    // Tarea para actualizar periódicamente
    xTaskCreate(UpdateAttributes, "UpdateAttributes", UPDATE_TASK_STACK_SIZE, NULL, UPDATE_TASK_PRIORITY, NULL);
}

void ApplicationShutdown()
{
    ESP_LOGD(TAG, "Electrical Sensor App: ApplicationShutdown()");
}

static void InitServer(intptr_t context)
{
    ESP_LOGI(TAG, "InitServer started");
    // Print QR Code URL
    PrintOnboardingCodes(chip::RendezvousInformationFlags(CONFIG_RENDEZVOUS_MODE));

    DeviceCallbacksDelegate::Instance().SetAppDelegate(&sAppDeviceCallbacksDelegate);
    Esp32AppServer::Init(); // Init ZCL Data Model and CHIP App Server AND
                            // Initialize device attestation config
#if CONFIG_ENABLE_ESP_INSIGHTS_TRACE
    esp_insights_config_t config = {
        .log_type = ESP_DIAG_LOG_TYPE_ERROR | ESP_DIAG_LOG_TYPE_WARNING | ESP_DIAG_LOG_TYPE_EVENT,
        .auth_key = insights_auth_key_start,
    };

    esp_err_t ret = esp_insights_init(&config);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ESP Insights, err:0x%x", ret);
    }

    static Tracing::Insights::ESP32Backend backend;
    Tracing::Register(backend);
#endif

    // Application code should always be initialised after the initialisation of
    // server.
    ESP_LOGI(TAG, "Calling ApplicationInit");
    ApplicationInit();
    ESP_LOGI(TAG, "ApplicationInit called");


#if CONFIG_ENABLE_SNTP_TIME_SYNC
    const char kNtpServerUrl[]             = "pool.ntp.org";
    const uint16_t kSyncNtpTimeIntervalDay = 1;
    chip::Esp32TimeSync::Init(kNtpServerUrl, kSyncNtpTimeIntervalDay);
#endif
}

extern "C" void app_main()
{
    esp_log_level_set("*", ESP_LOG_DEBUG);  
    // Initialize the ESP NVS layer.
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_flash_init() failed: %s", esp_err_to_name(err));
        return;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_loop_create_default() failed: %s", esp_err_to_name(err));
        return;
    }
#if CONFIG_ENABLE_PW_RPC
    chip::rpc::Init();
#endif

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "chip-esp32-electrical-sensor-example starting");
    ESP_LOGI(TAG, "==================================================");

#if CONFIG_ENABLE_CHIP_SHELL
#if CONFIG_OPENTHREAD_CLI
    chip::RegisterOpenThreadCliCommands();
#endif
    chip::LaunchShell();
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
    if (Internal::ESP32Utils::InitWiFiStack() != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to initialize WiFi stack");
        return;
    }
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

    DeviceLayer::SetDeviceInfoProvider(&gExampleDeviceInfoProvider);

    CHIPDeviceManager & deviceMgr = CHIPDeviceManager::GetInstance();
    CHIP_ERROR error              = deviceMgr.Init(&EchoCallbacks);
    if (error != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "device.Init() failed: %s", ErrorStr(error));
        return;
    }

#if CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER
    SetCommissionableDataProvider(&sFactoryDataProvider);
#if CONFIG_ENABLE_ESP32_DEVICE_INSTANCE_INFO_PROVIDER
    SetDeviceInstanceInfoProvider(&sFactoryDataProvider);
#endif
#endif

    SetDeviceAttestationCredentialsProvider(get_dac_provider());
    ESPOpenThreadInit();

    chip::DeviceLayer::PlatformMgr().ScheduleWork(InitServer, reinterpret_cast<intptr_t>(nullptr));

    // Inicializar el generador de números aleatorios
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

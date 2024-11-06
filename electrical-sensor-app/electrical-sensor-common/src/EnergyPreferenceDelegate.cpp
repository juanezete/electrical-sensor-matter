#include "EnergyPreferenceDelegate.h"
#include <lib/support/Span.h> 
#include "esp_log.h"

using namespace chip;

static const char * TAG = "EnergyPreferenceDelegate";

static uint8_t gCurrentEnergyBalance = 0;

CHIP_ERROR chip::app::Clusters::EnergyPreferenceDelegate::GetEnergyBalanceAtIndex(
    chip::EndpointId aEndpoint, size_t aIndex, chip::Percent & aOutStep,
    chip::Optional<chip::MutableCharSpan> & aOutLabel)
{
    if (aIndex >= GetNumEnergyBalances(aEndpoint))
    {
        return CHIP_ERROR_NOT_FOUND;
    }
    switch (aIndex)
    {
    case 0:
        aOutStep = 50; 
        break;
    case 1:
        aOutStep = 20; 
        break;
    case 2:
        aOutStep = 80; 
        break;
    default:
        return CHIP_ERROR_NOT_FOUND; 
    }

    ESP_LOGI(TAG, "Devolviendo Energy Balance para índice %zu: %u%%", aIndex, static_cast<unsigned>(aOutStep));
    return CHIP_NO_ERROR;
}

CHIP_ERROR chip::app::Clusters::EnergyPreferenceDelegate::GetEnergyPriorityAtIndex(
    chip::EndpointId aEndpoint, size_t aIndex,
    chip::app::Clusters::EnergyPreference::EnergyPriorityEnum & aOutPriority)
{
    aOutPriority = chip::app::Clusters::EnergyPreference::EnergyPriorityEnum::kEfficiency;
    return CHIP_NO_ERROR;
}

CHIP_ERROR chip::app::Clusters::EnergyPreferenceDelegate::GetLowPowerModeSensitivityAtIndex(
    chip::EndpointId aEndpoint, size_t aIndex, chip::Percent & aOutStep,
    chip::Optional<chip::MutableCharSpan> & aOutLabel)
{
    if (aIndex > 1) 
    {
        return CHIP_ERROR_NOT_FOUND;
    }

    aOutStep = (aIndex == 0) ? 10 : 90; 

    return CHIP_NO_ERROR;
}

size_t chip::app::Clusters::EnergyPreferenceDelegate::GetNumEnergyBalances(chip::EndpointId aEndpoint)
{
    return 3; 
}

size_t chip::app::Clusters::EnergyPreferenceDelegate::GetNumLowPowerModeSensitivities(chip::EndpointId aEndpoint)
{
    return 2; 
}

CHIP_ERROR chip::app::Clusters::EnergyPreferenceDelegate::WriteCurrentEnergyBalance(chip::EndpointId aEndpoint, uint8_t newBalance)
{
    if (newBalance > 2)
    {
        ESP_LOGE(TAG, "Valor inválido para CurrentEnergyBalance: %u", newBalance);
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // Actualizar el valor almacenado en RAM
    gCurrentEnergyBalance = newBalance;
    ESP_LOGI(TAG, "CurrentEnergyBalance actualizado a: %u", newBalance);

    return CHIP_NO_ERROR;
}
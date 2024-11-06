#include <DeviceEnergyManagementManager.h>
#include <ElectricalPowerMeasurementDelegate.h>
#include <PowerTopologyDelegate.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <app/clusters/electrical-power-measurement-server/electrical-power-measurement-server.h>
#include <app/clusters/power-topology-server/power-topology-server.h>
#include <app/server/Server.h>
#include <lib/support/logging/CHIPLogging.h>

#define ELECTRICAL_CONTROL_ENDPOINT 1

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::DeviceEnergyManagement;
using namespace chip::app::Clusters::ElectricalPowerMeasurement;
using namespace chip::app::Clusters::PowerTopology;

static std::unique_ptr<DeviceEnergyManagementDelegate> gDEMDelegate;
static std::unique_ptr<DeviceEnergyManagementManager> gDEMInstance;
static std::unique_ptr<ElectricalPowerMeasurementDelegate> gEPMDelegate;
static std::unique_ptr<ElectricalPowerMeasurementInstance> gEPMInstance;
static std::unique_ptr<PowerTopologyDelegate> gPTDelegate;
static std::unique_ptr<PowerTopologyInstance> gPTInstance;

CHIP_ERROR DeviceEnergyManagementInit()
{
    if (gDEMDelegate || gDEMInstance)
    {
        ChipLogError(AppServer, "DEM Instance or Delegate already exist.");
        return CHIP_ERROR_INCORRECT_STATE;
    }

    gDEMDelegate = std::make_unique<DeviceEnergyManagementDelegate>();
    if (!gDEMDelegate)
    {
        ChipLogError(AppServer, "Failed to allocate memory for DeviceEnergyManagementDelegate");
        return CHIP_ERROR_NO_MEMORY;
    }

    gDEMInstance = std::make_unique<DeviceEnergyManagementManager>(
        EndpointId(ELECTRICAL_CONTROL_ENDPOINT), *gDEMDelegate,
        BitMask<DeviceEnergyManagement::Feature, uint32_t>(
            DeviceEnergyManagement::Feature::kPowerAdjustment, DeviceEnergyManagement::Feature::kPowerForecastReporting,
            DeviceEnergyManagement::Feature::kStateForecastReporting, DeviceEnergyManagement::Feature::kStartTimeAdjustment,
            DeviceEnergyManagement::Feature::kPausable));

    if (!gDEMInstance)
    {
        ChipLogError(AppServer, "Failed to allocate memory for DeviceEnergyManagementManager");
        gDEMDelegate.reset();
        return CHIP_ERROR_NO_MEMORY;
    }

    CHIP_ERROR err = gDEMInstance->Init();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Init failed on gDEMInstance");
        gDEMInstance.reset();
        gDEMDelegate.reset();
        return err;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceEnergyManagementShutdown()
{
    if (gDEMInstance)
    {
        gDEMInstance->Shutdown();
        gDEMInstance.reset();
    }
    if (gDEMDelegate)
    {
        gDEMDelegate.reset();
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR ElectricalPowerMeasurementInit()
{
    CHIP_ERROR err;

    if (gEPMDelegate || gEPMInstance)
    {
        ChipLogError(AppServer, "EPM Instance or Delegate already exist.");
        return CHIP_ERROR_INCORRECT_STATE;
    }

    gEPMDelegate = std::make_unique<ElectricalPowerMeasurementDelegate>();
    if (!gEPMDelegate)
    {
        ChipLogError(AppServer, "Failed to allocate memory for EPM Delegate");
        return CHIP_ERROR_NO_MEMORY;
    }

    gEPMInstance = std::make_unique<ElectricalPowerMeasurementInstance>(
        EndpointId(ELECTRICAL_CONTROL_ENDPOINT), *gEPMDelegate,
        BitMask<ElectricalPowerMeasurement::Feature, uint32_t>(
            ElectricalPowerMeasurement::Feature::kDirectCurrent, ElectricalPowerMeasurement::Feature::kAlternatingCurrent,
            ElectricalPowerMeasurement::Feature::kPolyphasePower, ElectricalPowerMeasurement::Feature::kHarmonics,
            ElectricalPowerMeasurement::Feature::kPowerQuality),
        BitMask<ElectricalPowerMeasurement::OptionalAttributes, uint32_t>(
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeVoltage,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeActiveCurrent,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeReactiveCurrent,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeApparentCurrent,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeReactivePower,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeApparentPower,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeRMSVoltage,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeRMSCurrent,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeRMSPower,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeFrequency,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributePowerFactor,
            ElectricalPowerMeasurement::OptionalAttributes::kOptionalAttributeNeutralCurrent));

    if (!gEPMInstance)
    {
        ChipLogError(AppServer, "Failed to allocate memory for EPM Instance");
        gEPMDelegate.reset();
        return CHIP_ERROR_NO_MEMORY;
    }

    err = gEPMInstance->Init();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Init failed on gEPMInstance");
        gEPMInstance.reset();
        gEPMDelegate.reset();
        return err;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR ElectricalPowerMeasurementShutdown()
{
    if (gEPMInstance)
    {
        gEPMInstance->Shutdown();
        gEPMInstance.reset();
    }
    if (gEPMDelegate)
    {
        gEPMDelegate.reset();
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR PowerTopologyInit()
{
    CHIP_ERROR err;

    if (gPTDelegate || gPTInstance)
    {
        ChipLogError(AppServer, "PowerTopology Instance or Delegate already exist.");
        return CHIP_ERROR_INCORRECT_STATE;
    }

    gPTDelegate = std::make_unique<PowerTopologyDelegate>();
    if (!gPTDelegate)
    {
        ChipLogError(AppServer, "Failed to allocate memory for PowerTopology Delegate");
        return CHIP_ERROR_NO_MEMORY;
    }

    gPTInstance = std::make_unique<PowerTopologyInstance>(
        EndpointId(ELECTRICAL_CONTROL_ENDPOINT), *gPTDelegate,
        BitMask<PowerTopology::Feature, uint32_t>(PowerTopology::Feature::kNodeTopology),
        BitMask<PowerTopology::OptionalAttributes, uint32_t>(0));

    if (!gPTInstance)
    {
        ChipLogError(AppServer, "Failed to allocate memory for PowerTopology Instance");
        gPTDelegate.reset();
        return CHIP_ERROR_NO_MEMORY;
    }

    err = gPTInstance->Init();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Init failed on gPTInstance");
        gPTInstance.reset();
        gPTDelegate.reset();
        return err;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR PowerTopologyShutdown()
{
    if (gPTInstance)
    {
        gPTInstance->Shutdown();
        gPTInstance.reset();
    }
    if (gPTDelegate)
    {
        gPTDelegate.reset();
    }
    return CHIP_NO_ERROR;
}

void ElectricalControllerInit()
{
     ChipLogProgress(AppServer, "Initializing Device Energy Management");
    if (DeviceEnergyManagementInit() != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "DeviceEnergyManagementInit failed");
        return;
    }
    ChipLogProgress(AppServer, "Device Energy Management Initialized Successfully");

    ChipLogProgress(AppServer, "Initializing Electrical Power Measurement");
    if (ElectricalPowerMeasurementInit() != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "ElectricalPowerMeasurementInit failed");
        DeviceEnergyManagementShutdown();
        return;
    }
    ChipLogProgress(AppServer, "Electrical Power Measurement Initialized Successfully");

    ChipLogProgress(AppServer, "Initializing Power Topology");
    if (PowerTopologyInit() != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "PowerTopologyInit failed");
        ElectricalPowerMeasurementShutdown();
        DeviceEnergyManagementShutdown();
        return;
    }
    ChipLogProgress(AppServer, "Power Topology Initialized Successfully");
}

void ElectricalControllerShutdown()
{
    ChipLogDetail(AppServer, "Electrical Controller: ApplicationShutdown()");

    PowerTopologyShutdown();
    ElectricalPowerMeasurementShutdown();
    DeviceEnergyManagementShutdown();
}

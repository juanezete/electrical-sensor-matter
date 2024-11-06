#pragma once

#include <app/clusters/energy-preference-server/energy-preference-server.h>
#include <app/util/basic-types.h>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h> 

namespace chip {
namespace app {
namespace Clusters {

class EnergyPreferenceDelegate : public EnergyPreference::Delegate
{
public:
    CHIP_ERROR GetEnergyBalanceAtIndex(chip::EndpointId aEndpoint, size_t aIndex, chip::Percent & aOutStep,
                                       chip::Optional<chip::MutableCharSpan> & aOutLabel) override;

    CHIP_ERROR GetEnergyPriorityAtIndex(chip::EndpointId aEndpoint, size_t aIndex,
                                        chip::app::Clusters::EnergyPreference::EnergyPriorityEnum & aOutPriority) override;

    CHIP_ERROR GetLowPowerModeSensitivityAtIndex(chip::EndpointId aEndpoint, size_t aIndex, chip::Percent & aOutStep,
                                                 chip::Optional<chip::MutableCharSpan> & aOutLabel) override;

    size_t GetNumEnergyBalances(chip::EndpointId aEndpoint) override;

    size_t GetNumLowPowerModeSensitivities(chip::EndpointId aEndpoint) override;

    CHIP_ERROR WriteCurrentEnergyBalance(chip::EndpointId aEndpoint, uint8_t newBalance);
};

} // namespace Clusters
} // namespace app
} // namespace chip

/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "air_quality_matter_adapter.h"

#include "air_quality_policy.h"
#include "clusters/cluster_init.h"
#include "clusters/identify.h"
#include "measurement_service/measurement_service.h"

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/clusters/air-quality-server/air-quality-server.h>
#include <app/clusters/concentration-measurement-server/concentration-measurement-server.h>
#include <lib/support/BitMask.h>
#include <platform/PlatformManager.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

namespace AirQualityEndpoint {

namespace {

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

constexpr EndpointId kEndpointId = 1;

/*
 * NumericMeasurement only: SEN66 provides no level thresholds, peak, or
 * average statistics, so those Concentration Measurement cluster features
 * stay off - see docs/architecture.md.
 */
using ConcentrationInstance = ConcentrationMeasurement::Instance<true, false, false, false, false, false>;

Nrf::Matter::IdentifyCluster sIdentifyCluster(kEndpointId);

AirQuality::Instance sAirQualityInstance(
    kEndpointId, BitMask<AirQuality::Feature>(AirQuality::Feature::kFair, AirQuality::Feature::kModerate,
                                              AirQuality::Feature::kVeryPoor, AirQuality::Feature::kExtremelyPoor));

ConcentrationInstance sCo2Instance(kEndpointId, CarbonDioxideConcentrationMeasurement::Id,
                                   ConcentrationMeasurement::MeasurementMediumEnum::kAir,
                                   ConcentrationMeasurement::MeasurementUnitEnum::kPpm);
ConcentrationInstance sPm1Instance(kEndpointId, Pm1ConcentrationMeasurement::Id,
                                   ConcentrationMeasurement::MeasurementMediumEnum::kAir,
                                   ConcentrationMeasurement::MeasurementUnitEnum::kUgm3);
ConcentrationInstance sPm25Instance(kEndpointId, Pm25ConcentrationMeasurement::Id,
                                    ConcentrationMeasurement::MeasurementMediumEnum::kAir,
                                    ConcentrationMeasurement::MeasurementUnitEnum::kUgm3);
ConcentrationInstance sPm10Instance(kEndpointId, Pm10ConcentrationMeasurement::Id,
                                    ConcentrationMeasurement::MeasurementMediumEnum::kAir,
                                    ConcentrationMeasurement::MeasurementUnitEnum::kUgm3);

/*
 * Re-reads the measurement service's latest snapshot and republishes every
 * attribute. Runs on the CHIP thread (only ever invoked via ScheduleWork).
 * Re-fetching here instead of carrying the payload through ScheduleWork's
 * single intptr_t argument keeps this idempotent: several coalesced
 * ScheduleWork calls just reapply the same latest snapshot, and each
 * cluster's Set()/SetMeasuredValue() already no-ops on an unchanged value.
 */
void ApplyLatestMeasurement(intptr_t /* unused */) {
    MeasurementService::Snapshot snapshot;
    if (!MeasurementService::GetLatest(snapshot)) {
        return;
    }
    const Sen66::Measurement &m = snapshot.measurement;

    // A cleared valid bit is never read as a value (see sen66_protocol.h) -
    // it is published as Matter-null instead of a stale or zero reading.
    if (m.valid & Sen66::kFieldTemperature) {
        TemperatureMeasurement::Attributes::MeasuredValue::Set(kEndpointId,
                                                                TemperatureRawToMatterCentiDegC(m.temperature));
    } else {
        TemperatureMeasurement::Attributes::MeasuredValue::SetNull(kEndpointId);
    }

    if (m.valid & Sen66::kFieldHumidity) {
        RelativeHumidityMeasurement::Attributes::MeasuredValue::Set(kEndpointId,
                                                                     HumidityRawToMatterCentiPercent(m.humidity));
    } else {
        RelativeHumidityMeasurement::Attributes::MeasuredValue::SetNull(kEndpointId);
    }

    sCo2Instance.SetMeasuredValue((m.valid & Sen66::kFieldCo2) ? DataModel::MakeNullable(Co2RawToPpm(m.co2))
                                                                : DataModel::Nullable<float>());
    sPm1Instance.SetMeasuredValue((m.valid & Sen66::kFieldPm1p0) ? DataModel::MakeNullable(PmRawToUgm3(m.pm1p0))
                                                                  : DataModel::Nullable<float>());
    sPm25Instance.SetMeasuredValue((m.valid & Sen66::kFieldPm2p5) ? DataModel::MakeNullable(PmRawToUgm3(m.pm2p5))
                                                                   : DataModel::Nullable<float>());
    sPm10Instance.SetMeasuredValue((m.valid & Sen66::kFieldPm10) ? DataModel::MakeNullable(PmRawToUgm3(m.pm10))
                                                                  : DataModel::Nullable<float>());

    PolicyInputs inputs;
    inputs.co2Valid = (m.valid & Sen66::kFieldCo2) != 0;
    inputs.co2Raw = m.co2;
    inputs.pm2p5Valid = (m.valid & Sen66::kFieldPm2p5) != 0;
    inputs.pm2p5Raw = m.pm2p5;
    inputs.pm10Valid = (m.valid & Sen66::kFieldPm10) != 0;
    inputs.pm10Raw = m.pm10;

    AirQualityLevel level = EvaluateAirQuality(inputs);
    sAirQualityInstance.UpdateAirQuality(static_cast<AirQuality::AirQualityEnum>(level));
}

/*
 * Runs on the measurement service's own work-queue thread - see
 * docs/measurement-service.md's "Notify policy (for M4)". Must not touch any
 * Matter attribute directly; hop onto the CHIP thread first.
 */
void OnMeasurementUpdated(const Sen66::Measurement & /* snapshot */, uint16_t /* changedMask */) {
    chip::DeviceLayer::PlatformMgr().ScheduleWork(&ApplyLatestMeasurement, 0);
}

/*
 * Registered below via NRF_MATTER_CLUSTER_INIT, which nrf_matter_cluster_init_run_all()
 * calls from inside Nrf::Matter::StartServer() - specifically after
 * Server::GetInstance().Init() has already run (StartServer() calls
 * WaitForReadiness() first). Calling these cluster Init() methods any earlier
 * is unsafe: AirQuality::Instance::Init()/ConcentrationMeasurement::Instance::Init()
 * both hard-assert via VerifyOrDie(emberAfContainsServer(...)), and ember's
 * runtime endpoint table is only populated once Server::Init() has completed -
 * calling Init() from AppTask::Init() before StartServer() crashes the device
 * (emberAfContainsServer() reads an empty table and returns false). See
 * nrf/samples/matter/common/src/clusters/cluster_init.h for the mechanism.
 */
bool ClusterInit() {
    if (sIdentifyCluster.Init() != CHIP_NO_ERROR) {
        LOG_ERR("Failed to register Identify cluster on endpoint %u", kEndpointId);
        return false;
    }
    if (sAirQualityInstance.Init() != CHIP_NO_ERROR) {
        LOG_ERR("Failed to register Air Quality cluster on endpoint %u", kEndpointId);
        return false;
    }
    if (sCo2Instance.Init() != CHIP_NO_ERROR || sPm1Instance.Init() != CHIP_NO_ERROR ||
        sPm25Instance.Init() != CHIP_NO_ERROR || sPm10Instance.Init() != CHIP_NO_ERROR) {
        LOG_ERR("Failed to register a Concentration Measurement cluster on endpoint %u", kEndpointId);
        return false;
    }

    MeasurementService::RegisterUpdateCallback(&OnMeasurementUpdated);

    // Publish whatever the measurement service already has, if anything,
    // instead of waiting for the first notify-worthy update.
    ApplyLatestMeasurement(0);

    return true;
}

} // namespace

NRF_MATTER_CLUSTER_INIT(air_quality_endpoint, ClusterInit);

} // namespace AirQualityEndpoint

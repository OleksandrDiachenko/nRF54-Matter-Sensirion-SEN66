/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

/*
 * Registers endpoint 1's Identify, Air Quality, Temperature Measurement,
 * Relative Humidity Measurement, and CO2/PM1/PM2.5/PM10 Concentration
 * Measurement clusters, and subscribes to the measurement service so it can
 * keep them in sync.
 *
 * This module self-registers via NRF_MATTER_CLUSTER_INIT (see
 * air_quality_matter_adapter.cpp and
 * nrf/samples/matter/common/src/clusters/cluster_init.h) - nothing in
 * app_task.cpp needs to call into it directly. Nrf::Matter::StartServer()
 * runs its registration callback automatically, at the one point in startup
 * where this is safe: after Server::GetInstance().Init() has completed (so
 * ember's endpoint tables exist) and before any external Matter traffic can
 * arrive.
 *
 * This module is the only place in the application that writes Matter
 * attributes; every write after registration happens inside a
 * PlatformMgr().ScheduleWork() callback, i.e. in the Matter stack's own
 * thread context - see docs/architecture.md.
 */

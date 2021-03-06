/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/ras/os_ras.h"

namespace L0 {

class WddmRasImp : public OsRas {
    ze_result_t osRasGetProperties(zes_ras_properties_t &properties) override;
    ze_result_t osRasGetState(zes_ras_state_t &state, ze_bool_t clear) override;
};

ze_result_t OsRas::getSupportedRasErrorTypes(std::vector<zes_ras_error_type_t> &errorType, OsSysman *pOsSysman, ze_device_handle_t deviceHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmRasImp::osRasGetProperties(zes_ras_properties_t &properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmRasImp::osRasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsRas *OsRas::create(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    WddmRasImp *pWddmRasImp = new WddmRasImp();
    return static_cast<OsRas *>(pWddmRasImp);
}

} // namespace L0

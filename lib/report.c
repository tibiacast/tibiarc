/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2024 "John Högberg"
 *
 * This file is part of tibiarc.
 *
 * tibiarc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * tibiarc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with tibiarc. If not, see <https://www.gnu.org/licenses/>.
 */

#include "report.h"

#if defined(DISABLE_ERROR_REPORTING)
bool trc_GetLastError(size_t size, char *result) {
    abort();
    return false;
}

enum TrcErrorReportMode trc_ChangeErrorReporting(enum TrcErrorReportMode mode) {
    (void)mode;
    return TRC_ERROR_REPORT_MODE_NONE;
}
#else

#    include <string.h>
#    include <stdio.h>

#    include "utils.h"

#    ifdef __cplusplus
#        define _Thread_local thread_local
#    endif

static _Thread_local struct {
    enum TrcErrorReportMode Mode;
    size_t Position;
    char MessageBuffer[1024];
} LastError;

void _trc_ReportError(const char *format, ...) {
    va_list argList;

    va_start(argList, format);

    switch (LastError.Mode) {
    case TRC_ERROR_REPORT_MODE_NONE:
        break;
    case TRC_ERROR_REPORT_MODE_ABORT:
        abort();
        break;
    case TRC_ERROR_REPORT_MODE_TEXT: {
        int remaining = sizeof(LastError.MessageBuffer) - LastError.Position;
        if (remaining > 0) {
            int printed =
                    vsnprintf(&LastError.MessageBuffer[LastError.Position],
                              remaining,
                              format,
                              argList);
            ABORT_UNLESS(printed > 0);
            LastError.Position += printed;
            remaining -= printed;

            if (remaining <= 0) {
                LastError.Position = sizeof(LastError.MessageBuffer);
            }
        }
    }
    }
}

bool trc_GetLastError(size_t size, char *result) {
    if (LastError.Mode == TRC_ERROR_REPORT_MODE_TEXT) {
        ABORT_UNLESS(LastError.Position > 0);

        size_t count = MIN(size - 1, LastError.Position);
        memcpy(result, LastError.MessageBuffer, count);
        result[count] = 0;

        LastError.Position = 0;
        return true;
    }

    ABORT_UNLESS(LastError.Position == 0);
    return false;
}

enum TrcErrorReportMode trc_ChangeErrorReporting(enum TrcErrorReportMode mode) {
    enum TrcErrorReportMode previous;

    ABORT_UNLESS(LastError.Position == 0);
    previous = LastError.Mode;
    LastError.Mode = mode;

    return previous;
}

#endif

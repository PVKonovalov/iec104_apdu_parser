/*
 *  Copyright 2026 Pavel Konovalov
 *
 *  This file is part of iec104_apdu_parser
 *
 *  iec104_apdu_parser is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  iec104_apdu_parser is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with iec104_apdu_parser. If not, see <http://www.gnu.org/licenses/>.
 *
 *  See LICENSE file for the complete license text.
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cctype>

#include "cppflags/cppflags.h"

extern "C" {
#include "iec60870_common.h"
#include "cs101_information_objects.h"
#include "cs101_asdu_internal.h"
}

// ── helpers ───────────────────────────────────────────────────────────────────────────────────────────────────────────

static std::string hex2(uint8_t b) {
    std::ostringstream os;
    os << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return os.str();
}

static std::string hexBytes(const uint8_t *buf, int len) {
    std::ostringstream os;
    for (int i = 0; i < len; ++i) {
        if (i) os << ' ';
        os << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buf[i]);
    }
    return os.str();
}

// Strip spaces, colons, dashes; validate hex characters
static std::vector<uint8_t> parseHexString(const std::string &input, std::vector<std::string> &errors) {
    std::string clean;
    for (size_t i = 0; i < input.size(); ++i) {
        if (char c = input[i]; std::isxdigit(static_cast<unsigned char>(c))) {
            clean += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        } else if (c == ' ' || c == '\t' || c == ':' || c == '-') {
            // acceptable separators – ignore
        } else {
            std::ostringstream msg;
            msg << "Invalid character '" << c << "' at position " << i;
            errors.push_back(msg.str());
        }
    }

    if (clean.empty()) {
        errors.emplace_back("Input is empty");
        return {};
    }

    if (clean.size() % 2 != 0) {
        errors.emplace_back("Odd number of hex nibbles – leading '0' assumed");
        clean = "0" + clean;
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(clean.size() / 2);
    for (size_t i = 0; i < clean.size(); i += 2)
        bytes.push_back(static_cast<uint8_t>(std::stoul(clean.substr(i, 2), nullptr, 16)));
    return bytes;
}

// ── quality descriptor printer ────────────────────────────────────────────────────────────────────────────────────────

static std::string qualityStr(QualityDescriptor qd) {
    if (qd == IEC60870_QUALITY_GOOD) return "GOOD";
    std::string s;
    if (qd & IEC60870_QUALITY_OVERFLOW) s += "OV|";
    if (qd & IEC60870_QUALITY_BLOCKED) s += "BL|";
    if (qd & IEC60870_QUALITY_SUBSTITUTED) s += "SB|";
    if (qd & IEC60870_QUALITY_NON_TOPICAL) s += "NT|";
    if (qd & IEC60870_QUALITY_INVALID) s += "IV|";
    if (!s.empty()) s.pop_back();
    return s;
}

// ── CP56Time2a printer ────────────────────────────────────────────────────────────────────────────────────────────────

static std::string cp56Str(CP56Time2a t) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d%s",
                  2000 + CP56Time2a_getYear(t), CP56Time2a_getMonth(t), CP56Time2a_getDayOfMonth(t),
                  CP56Time2a_getHour(t), CP56Time2a_getMinute(t), CP56Time2a_getSecond(t),
                  CP56Time2a_getMillisecond(t), CP56Time2a_isInvalid(t) ? " [INVALID]" : "");
    return buf;
}

// ── information object printer ────────────────────────────────────────────────────────────────────────────────────────

static void printInfoObject(InformationObject io, TypeID tid, int idx, bool unsignedSVA) {
    if (!io) return;
    std::cout << "    [" << idx << "] IOA=" << InformationObject_getObjectAddress(io) << "\n";

    switch (tid) {
        case M_SP_NA_1: {
            auto *sp = reinterpret_cast<SinglePointInformation>(io);
            std::cout << "        SPI   = " << (SinglePointInformation_getValue(sp) ? "ON" : "OFF") << "\n";
            std::cout << "        QDS   = " << qualityStr(SinglePointInformation_getQuality(sp)) << "\n";
            break;
        }
        case M_SP_TB_1: {
            auto *sp = reinterpret_cast<SinglePointInformation>(io);
            auto *spt = reinterpret_cast<SinglePointWithCP56Time2a>(io);
            std::cout << "        SPI   = " << (SinglePointInformation_getValue(sp) ? "ON" : "OFF") << "\n";
            std::cout << "        QDS   = " << qualityStr(SinglePointInformation_getQuality(sp)) << "\n";
            std::cout << "        Time  = " << cp56Str(SinglePointWithCP56Time2a_getTimestamp(spt)) << "\n";
            break;
        }

        case M_DP_NA_1: {
            auto *dp = reinterpret_cast<DoublePointInformation>(io);
            static const char *dpv[] = {"INTERMEDIATE", "OFF", "ON", "INDETERMINATE"};
            std::cout << "        DPI   = " << dpv[DoublePointInformation_getValue(dp)] << "\n";
            std::cout << "        QDS   = " << qualityStr(DoublePointInformation_getQuality(dp)) << "\n";
            break;
        }
        case M_DP_TB_1: {
            auto *dp = reinterpret_cast<DoublePointInformation>(io);
            auto *dpt = reinterpret_cast<DoublePointWithCP56Time2a>(io);
            static const char *dpv[] = {"INTERMEDIATE", "OFF", "ON", "INDETERMINATE"};
            std::cout << "        DPI   = " << dpv[DoublePointInformation_getValue(dp)] << "\n";
            std::cout << "        QDS   = " << qualityStr(DoublePointInformation_getQuality(dp)) << "\n";
            std::cout << "        Time  = " << cp56Str(DoublePointWithCP56Time2a_getTimestamp(dpt)) << "\n";
            break;
        }

        case M_ST_NA_1: {
            auto *st = reinterpret_cast<StepPositionInformation>(io);
            std::cout << "        VTI   = " << StepPositionInformation_getValue(st) << (StepPositionInformation_isTransient(st) ? " [TRANSIENT]" : "") << "\n";
            std::cout << "        QDS   = " << qualityStr(StepPositionInformation_getQuality(st)) << "\n";
            break;
        }
        case M_ST_TB_1: {
            auto *st = reinterpret_cast<StepPositionInformation>(io);
            auto *stt = reinterpret_cast<StepPositionWithCP56Time2a>(io);
            std::cout << "        VTI   = " << StepPositionInformation_getValue(st) << (StepPositionInformation_isTransient(st) ? " [TRANSIENT]" : "") << "\n";
            std::cout << "        QDS   = " << qualityStr(StepPositionInformation_getQuality(st)) << "\n";
            std::cout << "        Time  = " << cp56Str(StepPositionWithCP56Time2a_getTimestamp(stt)) << "\n";
            break;
        }

        case M_BO_NA_1: {
            auto *bs = reinterpret_cast<BitString32>(io);
            std::cout << "        BSI   = 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << BitString32_getValue(bs) << std::dec << "\n";
            std::cout << "        QDS   = " << qualityStr(BitString32_getQuality(bs)) << "\n";
            break;
        }
        case M_BO_TB_1: {
            auto *bs = reinterpret_cast<BitString32>(io);
            auto *bst = reinterpret_cast<Bitstring32WithCP56Time2a>(io);
            std::cout << "        BSI   = 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << BitString32_getValue(bs) << std::dec << "\n";
            std::cout << "        QDS   = " << qualityStr(BitString32_getQuality(bs)) << "\n";
            std::cout << "        Time  = " << cp56Str(Bitstring32WithCP56Time2a_getTimestamp(bst)) << "\n";
            break;
        }

        case M_ME_NA_1: {
            auto *mv = reinterpret_cast<MeasuredValueNormalized>(io);
            std::cout << "        NVA   = " << MeasuredValueNormalized_getValue(mv) << "\n";
            std::cout << "        QDS   = " << qualityStr(MeasuredValueNormalized_getQuality(mv)) << "\n";
            break;
        }
        case M_ME_TD_1: {
            auto *mv = reinterpret_cast<MeasuredValueNormalized>(io);
            auto *mvt = reinterpret_cast<MeasuredValueNormalizedWithCP56Time2a>(io);
            std::cout << "        NVA   = " << MeasuredValueNormalized_getValue(mv) << "\n";
            std::cout << "        QDS   = " << qualityStr(MeasuredValueNormalized_getQuality(mv)) << "\n";
            std::cout << "        Time  = " << cp56Str(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(mvt)) << "\n";
            break;
        }

        case M_ME_NB_1: {
            auto *mv = reinterpret_cast<MeasuredValueScaled>(io);
            int sva_nb1 = MeasuredValueScaled_getValue(mv);
            std::cout << "        SVA   = " << (unsignedSVA ? static_cast<int>(static_cast<uint16_t>(sva_nb1)) : sva_nb1) << "\n";
            std::cout << "        QDS   = " << qualityStr(MeasuredValueScaled_getQuality(mv)) << "\n";
            break;
        }
        case M_ME_TE_1: {
            auto *mv = reinterpret_cast<MeasuredValueScaled>(io);
            auto *mvt = reinterpret_cast<MeasuredValueScaledWithCP56Time2a>(io);
            int sva_te1 = MeasuredValueScaled_getValue(mv);
            std::cout << "        SVA   = " << (unsignedSVA ? static_cast<int>(static_cast<uint16_t>(sva_te1)) : sva_te1) << "\n";
            std::cout << "        QDS   = " << qualityStr(MeasuredValueScaled_getQuality(mv)) << "\n";
            std::cout << "        Time  = " << cp56Str(MeasuredValueScaledWithCP56Time2a_getTimestamp(mvt)) << "\n";
            break;
        }

        case M_ME_NC_1: {
            auto *mv = reinterpret_cast<MeasuredValueShort>(io);
            std::cout << "        Value = " << MeasuredValueShort_getValue(mv) << "\n";
            std::cout << "        QDS   = " << qualityStr(MeasuredValueShort_getQuality(mv)) << "\n";
            break;
        }
        case M_ME_TF_1: {
            auto *mv = reinterpret_cast<MeasuredValueShort>(io);
            auto *mvt = reinterpret_cast<MeasuredValueShortWithCP56Time2a>(io);
            std::cout << "        Value = " << MeasuredValueShort_getValue(mv) << "\n";
            std::cout << "        QDS   = " << qualityStr(MeasuredValueShort_getQuality(mv)) << "\n";
            std::cout << "        Time  = " << cp56Str(MeasuredValueShortWithCP56Time2a_getTimestamp(mvt)) << "\n";
            break;
        }

        case M_IT_NA_1: {
            auto *it = reinterpret_cast<IntegratedTotals>(io);
            BinaryCounterReading bcr = IntegratedTotals_getBCR(it);
            std::cout << "        BCR   = " << BinaryCounterReading_getValue(bcr) << "  seq=" << BinaryCounterReading_getSequenceNumber(bcr) << "\n";
            break;
        }
        case M_IT_TB_1: {
            auto *it = reinterpret_cast<IntegratedTotals>(io);
            auto *itt = reinterpret_cast<IntegratedTotalsWithCP56Time2a>(io);
            BinaryCounterReading bcr = IntegratedTotals_getBCR(it);
            std::cout << "        BCR   = " << BinaryCounterReading_getValue(bcr) << "  seq=" << BinaryCounterReading_getSequenceNumber(bcr) << "\n";
            std::cout << "        Time  = " << cp56Str(IntegratedTotalsWithCP56Time2a_getTimestamp(itt)) << "\n";
            break;
        }

        case M_EI_NA_1: {
            auto *ei = reinterpret_cast<EndOfInitialization>(io);
            uint8_t coi = EndOfInitialization_getCOI(ei);
            std::cout << "        COI   = " << static_cast<int>(coi);
            switch (coi & 0x7F) {
                case IEC60870_COI_LOCAL_SWITCH_ON: std::cout << " (local power on)";
                    break;
                case IEC60870_COI_LOCAL_MANUAL_RESET: std::cout << " (local manual reset)";
                    break;
                case IEC60870_COI_REMOTE_RESET: std::cout << " (remote reset)";
                    break;
                default: break;
            }
            if (coi & 0x80) std::cout << " [initialisation from local parameter change]";
            std::cout << "\n";
            break;
        }

        case C_IC_NA_1: {
            auto *ic = reinterpret_cast<InterrogationCommand>(io);
            uint8_t qoi = InterrogationCommand_getQOI(ic);
            std::cout << "        QOI   = " << static_cast<int>(qoi);
            if (qoi == IEC60870_QOI_STATION) std::cout << " (station interrogation)";
            else if (qoi >= IEC60870_QOI_GROUP_1 && qoi <= 36) std::cout << " (group " << (qoi - 20) << ")";
            std::cout << "\n";
            break;
        }

        case C_CI_NA_1: {
            auto *ci = reinterpret_cast<CounterInterrogationCommand>(io);
            std::cout << "        QCC   = " << hex2(CounterInterrogationCommand_getQCC(ci)) << "\n";
            break;
        }

        case C_RD_NA_1:
            std::cout << "        (read command – no qualifier)\n";
            break;

        case C_CS_NA_1: {
            auto *cs = reinterpret_cast<ClockSynchronizationCommand>(io);
            std::cout << "        Time  = " << cp56Str(ClockSynchronizationCommand_getTime(cs)) << "\n";
            break;
        }

        case C_RP_NA_1: {
            auto *rp = reinterpret_cast<ResetProcessCommand>(io);
            std::cout << "        QRP   = " << static_cast<int>(ResetProcessCommand_getQRP(rp)) << "\n";
            break;
        }

        case C_SC_NA_1: {
            auto *sc = reinterpret_cast<SingleCommand>(io);
            std::cout << "        SCO   = " << (SingleCommand_getState(sc) ? "ON" : "OFF") << (SingleCommand_isSelect(sc) ? " [SELECT]" : " [EXECUTE]") << "\n";
            break;
        }
        case C_SC_TA_1: {
            auto *sc = reinterpret_cast<SingleCommand>(io);
            auto *sct = reinterpret_cast<SingleCommandWithCP56Time2a>(io);
            std::cout << "        SCO   = " << (SingleCommand_getState(sc) ? "ON" : "OFF") << (SingleCommand_isSelect(sc) ? " [SELECT]" : " [EXECUTE]") << "\n";
            std::cout << "        Time  = " << cp56Str(SingleCommandWithCP56Time2a_getTimestamp(sct)) << "\n";
            break;
        }

        case C_DC_NA_1: {
            auto *dc = reinterpret_cast<DoubleCommand>(io);
            static const char *dcv[] = {"NOT PERMITTED", "OFF", "ON", "NOT PERMITTED"};
            std::cout << "        DCO   = " << dcv[DoubleCommand_getState(dc) & 3] << (DoubleCommand_isSelect(dc) ? " [SELECT]" : " [EXECUTE]") << "\n";
            break;
        }
        case C_DC_TA_1: {
            auto *dc = reinterpret_cast<DoubleCommandWithCP56Time2a>(io);
            static const char *dcv[] = {"NOT PERMITTED", "OFF", "ON", "NOT PERMITTED"};
            std::cout << "        DCO   = " << dcv[DoubleCommandWithCP56Time2a_getState(dc) & 3] << (DoubleCommandWithCP56Time2a_isSelect(dc) ? " [SELECT]" : " [EXECUTE]") << "\n";
            std::cout << "        Time  = " << cp56Str(DoubleCommandWithCP56Time2a_getTimestamp(dc)) << "\n";
            break;
        }

        case C_RC_NA_1: {
            auto *rc = reinterpret_cast<StepCommand>(io);
            static const char *rcv[] = {"INVALID(0)", "LOWER", "HIGHER", "INVALID(3)"};
            std::cout << "        RCO   = " << rcv[StepCommand_getState(rc) & 3] << (StepCommand_isSelect(rc) ? " [SELECT]" : " [EXECUTE]") << "\n";
            break;
        }

        case C_SE_NA_1: {
            auto *se = reinterpret_cast<SetpointCommandNormalized>(io);
            std::cout << "        NVA   = " << SetpointCommandNormalized_getValue(se) << "\n";
            std::cout << "        QL    = " << static_cast<int>(SetpointCommandNormalized_getQL(se)) << (SetpointCommandNormalized_isSelect(se) ? " [SELECT]" : " [EXECUTE]") <<
                    "\n";
            break;
        }
        case C_SE_NB_1: {
            auto *se = reinterpret_cast<SetpointCommandScaled>(io);
            int sva_se = SetpointCommandScaled_getValue(se);
            std::cout << "        SVA   = " << (unsignedSVA ? static_cast<int>(static_cast<uint16_t>(sva_se)) : sva_se) << "\n";
            std::cout << "        QL    = " << static_cast<int>(SetpointCommandScaled_getQL(se)) << (SetpointCommandScaled_isSelect(se) ? " [SELECT]" : " [EXECUTE]") << "\n";
            break;
        }
        case C_SE_NC_1: {
            auto *se = reinterpret_cast<SetpointCommandShort>(io);
            std::cout << "        Value = " << SetpointCommandShort_getValue(se) << "\n";
            std::cout << "        QL    = " << static_cast<int>(SetpointCommandShort_getQL(se)) << (SetpointCommandShort_isSelect(se) ? " [SELECT]" : " [EXECUTE]") << "\n";
            break;
        }

        case P_ME_NA_1: {
            auto *pm = reinterpret_cast<ParameterNormalizedValue>(io);
            std::cout << "        NVA   = " << ParameterNormalizedValue_getValue(pm) << "\n";
            std::cout << "        QPM   = " << static_cast<int>(ParameterNormalizedValue_getQPM(pm)) << "\n";
            break;
        }
        case P_ME_NB_1: {
            auto *pm = reinterpret_cast<ParameterScaledValue>(io);
            int sva_pm = ParameterScaledValue_getValue(pm);
            std::cout << "        SVA   = " << (unsignedSVA ? static_cast<int>(static_cast<uint16_t>(sva_pm)) : sva_pm) << "\n";
            std::cout << "        QPM   = " << static_cast<int>(ParameterScaledValue_getQPM(pm)) << "\n";
            break;
        }
        case P_ME_NC_1: {
            auto *pm = reinterpret_cast<ParameterFloatValue>(io);
            std::cout << "        Value = " << ParameterFloatValue_getValue(pm) << "\n";
            std::cout << "        QPM   = " << static_cast<int>(ParameterFloatValue_getQPM(pm)) << "\n";
            break;
        }
        case P_AC_NA_1: {
            auto *pa = reinterpret_cast<ParameterActivation>(io);
            std::cout << "        QPA   = " << static_cast<int>(ParameterActivation_getQuality(pa)) << "\n";
            break;
        }

        default:
            std::cout << "        (detailed decoding not available for type " << static_cast<int>(tid) << ")\n";
            break;
    }

    InformationObject_destroy(io);
}

// ── ASDU printer ──────────────────────────────────────────────────────────────────────────────────────────────────────

static void printASdu(const uint8_t *asduBuf, int asduLen, const sCS101_AppLayerParameters &params, std::vector<std::string> &errors, bool unsignedSVA) {
    if (asduLen < 1) {
        errors.emplace_back("ASDU too short (0 bytes)");
        return;
    }

    int minHeaderLen = params.sizeOfTypeId + params.sizeOfVSQ + params.sizeOfCOT + params.sizeOfCA;
    if (asduLen < minHeaderLen) {
        errors.emplace_back("ASDU too short for header");
        return;
    }

    auto typeId = static_cast<TypeID>(asduBuf[0]);
    uint8_t vsq = asduBuf[1];
    bool isSeq = (vsq & 0x80) != 0;
    int numObj = vsq & 0x7F;
    uint8_t cotByte = asduBuf[2];
    bool isTest = (cotByte & 0x80) != 0;
    bool isNeg = (cotByte & 0x40) != 0;
    int cotVal = cotByte & 0x3F;
    int oa = (params.sizeOfCOT == 2) ? asduBuf[3] : 0;
    int caOff = params.sizeOfTypeId + params.sizeOfVSQ + params.sizeOfCOT;
    int ca = asduBuf[caOff] | (asduBuf[caOff + 1] << 8);
    auto cot = static_cast<CS101_CauseOfTransmission>(cotVal);

    std::cout << "  ┌─ ASDU ─────────────────────────────────────────\n";
    std::cout << "  │  Type ID : " << static_cast<int>(typeId) << " (" << TypeID_toString(typeId) << ")\n";
    std::cout << "  │  VSQ     : numObj=" << numObj << "  SQ=" << (isSeq ? "1 (sequence)" : "0") << "\n";
    std::cout << "  │  COT     : " << cotVal << " (" << CS101_CauseOfTransmission_toString(cot) << ")" << (isTest ? "  T=1" : "") << (isNeg ? "  P/N=1" : "") << "\n";
    if (params.sizeOfCOT == 2) std::cout << "  │  OA      : " << oa << "\n";
    std::cout << "  │  CA      : " << ca << "\n";
    std::cout << "  │  Objects : " << numObj << "\n";

    if (typeId > 127) errors.emplace_back("TypeID > 127 is in private range");
    if (numObj == 0) errors.emplace_back("Number of objects = 0 in ASDU");

    // C API takes non-const pointer – copy params
    sCS101_AppLayerParameters mp = params;
    CS101_ASDU asdu = CS101_ASDU_createFromBuffer(&mp, const_cast<uint8_t *>(asduBuf), asduLen); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    if (!asdu) {
        errors.emplace_back("Failed to create ASDU from buffer");
        return;
    }

    int actualCount = CS101_ASDU_getNumberOfElements(asdu);
    if (actualCount != numObj) {
        std::ostringstream msg;
        msg << "VSQ declares " << numObj << " objects but library parsed " << actualCount;
        errors.push_back(msg.str());
    }

    for (int i = 0; i < actualCount; ++i)
        printInfoObject(CS101_ASDU_getElement(asdu, i), typeId, i, unsignedSVA);

    CS101_ASDU_destroy(asdu);
    std::cout << "  └─────────────────────────────────────────────────\n";
}

// ── U-frame function name ─────────────────────────────────────────────────────────────────────────────────────────────

static const char *uFrameName(uint8_t cf1) {
    switch (cf1) {
        case 0x07: return "STARTDT act";
        case 0x0B: return "STARTDT con";
        case 0x13: return "STOPDT act";
        case 0x23: return "STOPDT con";
        case 0x43: return "TESTFR act";
        case 0x83: return "TESTFR con";
        default: return "UNKNOWN";
    }
}

// ── Main APDU parser ──────────────────────────────────────────────────────────────────────────────────────────────────

static void parseAPDU(const std::vector<uint8_t> &bytes, const sCS101_AppLayerParameters &params, std::vector<std::string> &errors, bool unsignedSVA) {
    const int totalLen = static_cast<int>(bytes.size());

    std::cout << "Raw bytes (" << totalLen << "): " << hexBytes(bytes.data(), totalLen) << "\n\n";

    if (totalLen < 2) {
        errors.emplace_back("Too short: need at least 2 bytes (start + length)");
        return;
    }

    if (bytes[0] != 0x68) {
        errors.emplace_back("Start byte is not 0x68 (IEC 104 APDU start)");
    } else {
        std::cout << "Start byte : 0x68  ✓\n";
    }

    int declaredLen = bytes[1];
    std::cout << "Length     : " << declaredLen << " (declared)  " << (totalLen - 2) << " (actual remaining)\n";

    if (declaredLen != totalLen - 2) {
        std::ostringstream msg;
        msg << "Length field=" << declaredLen << " but actual remaining bytes=" << (totalLen - 2);
        errors.push_back(msg.str());
    }

    if (totalLen < 6) {
        errors.emplace_back("Too short: APDU must have at least 6 bytes (start+len+4 APCI)");
        return;
    }

    uint8_t cf1 = bytes[2], cf2 = bytes[3], cf3 = bytes[4], cf4 = bytes[5];
    std::cout << "APCI ctrl  : " << hexBytes(bytes.data() + 2, 4) << "  (CF1-CF4)\n";

    if ((cf1 & 0x01) == 0) {
        int ns = ((cf1 >> 1) & 0x7F) | (cf2 << 7);
        int nr = ((cf3 >> 1) & 0x7F) | (cf4 << 7);
        std::cout << "\nFrame type : I-frame (Information)\n";
        std::cout << "  N(S)     : " << ns << "\n";
        std::cout << "  N(R)     : " << nr << "\n";

        int asduLen = totalLen - 6;
        if (asduLen < 1) {
            errors.emplace_back("I-frame has no ASDU payload");
            return;
        }
        std::cout << "\n";
        printASdu(bytes.data() + 6, asduLen, params, errors, unsignedSVA);
    } else if ((cf1 & 0x03) == 0x01) {
        int nr = ((cf3 >> 1) & 0x7F) | (cf4 << 7);
        std::cout << "\nFrame type : S-frame (Supervisory)\n";
        std::cout << "  N(R)     : " << nr << "\n";
        if (cf2 != 0x00) errors.emplace_back("S-frame CF2 should be 0x00");
        if (totalLen != 6) {
            std::ostringstream m;
            m << "S-frame should be exactly 6 bytes, got " << totalLen;
            errors.push_back(m.str());
        }
    } else {
        std::cout << "\nFrame type : U-frame (Unnumbered)\n";
        std::cout << "  Function : " << uFrameName(cf1) << "\n";
        if (cf2 != 0x00 || cf3 != 0x00 || cf4 != 0x00) errors.emplace_back("U-frame CF2/CF3/CF4 should all be 0x00");
        if (totalLen != 6) {
            std::ostringstream m;
            m << "U-frame should be exactly 6 bytes, got " << totalLen;
            errors.push_back(m.str());
        }
    }
}

// ── Dialog loop ───────────────────────────────────────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    sCS101_AppLayerParameters params{};
    params.sizeOfTypeId = 1;
    params.sizeOfVSQ = 1;
    params.sizeOfCOT = 2;
    params.originatorAddress = 0;
    params.sizeOfCA = 2;
    params.sizeOfIOA = 3;
    params.maxSizeOfASDU = 249;

    bool unsignedSVA = false;

    cppflags::FlagSet flags;
    flags.Bool("unsignedSVA", &unsignedSVA, "Display SVA (scaled value) as unsigned uint16 instead of signed int16");
    flags.Int("sizeOfCOT", &params.sizeOfCOT, params.sizeOfCOT, "Size of Cause of Transmission field (1 = no OA, 2 = with OA)");
    flags.Int("originatorAddress", &params.originatorAddress, params.originatorAddress, "Originator address used when sizeOfCOT = 2 (0-255)");
    flags.Int("sizeOfCA", &params.sizeOfCA, params.sizeOfCA, "Size of Common Address field in bytes (1 or 2)");
    flags.Int("sizeOfIOA", &params.sizeOfIOA, params.sizeOfIOA, "Size of Information Object Address field in bytes (1, 2, or 3)");
    flags.Int("maxSizeOfASDU", &params.maxSizeOfASDU, params.maxSizeOfASDU, "Maximum ASDU size in bytes (up to 249 for IEC 104)");

    try {
        flags.Parse(argc, argv);
    } catch (const cppflags::ParseError &e) {
        std::cerr << "Error: " << e.what() << "\n";
        flags.printUsage(argv[0]);
        return 1;
    }

    std::cout << "IEC 60870-5-104 APDU Parser\n";
    std::cout << "Parameters: sizeOfCOT=" << params.sizeOfCOT << " sizeOfCA=" << params.sizeOfCA << " sizeOfIOA=" << params.sizeOfIOA << " OA=" << params.originatorAddress << "\n";
    std::cout << "Enter hex bytes (spaces/colons allowed), empty line to quit.\n";
    std::cout << "Example: 68 0e 00 00 00 00 64 01 06 00 01 00 00 00 00 14\n";
    std::cout << std::string(60, '=') << "\n\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;

        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            std::cout << "Goodbye.\n";
            break;
        }

        std::vector<std::string> errors;
        std::vector<uint8_t> bytes = parseHexString(line, errors);

        if (!errors.empty() && bytes.empty()) {
            for (auto &e: errors) std::cout << "ERROR: " << e << "\n";
            std::cout << "\n";
            continue;
        }

        std::cout << std::string(60, '-') << "\n";
        parseAPDU(bytes, params, errors, unsignedSVA);

        if (!errors.empty()) {
            std::cout << "\n";
            for (auto &e: errors) std::cout << "⚠  ERROR: " << e << "\n";
        }
        std::cout << std::string(60, '=') << "\n\n";
    }

    return 0;
}

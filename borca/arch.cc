/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2018  Clifford Wolf <clifford@symbioticeda.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <iostream>
#include <math.h>
#include "nextpnr.h"
#include "placer1.h"
#include "placer_heap.h"
#include "router1.h"
#include "router2.h"
#include "util.h"

NEXTPNR_NAMESPACE_BEGIN

WireInfo &Arch::wire_info(IdString wire)
{
    auto w = wires.find(wire);
    if (w == wires.end())
        NPNR_ASSERT_FALSE_STR("no wire named " + wire.str(this));
    return w->second;
}

PipInfo &Arch::pip_info(IdString pip)
{
    auto p = pips.find(pip);
    if (p == pips.end())
        NPNR_ASSERT_FALSE_STR("no pip named " + pip.str(this));
    return p->second;
}

BelInfo &Arch::bel_info(IdString bel)
{
    auto b = bels.find(bel);
    if (b == bels.end())
        NPNR_ASSERT_FALSE_STR("no bel named " + bel.str(this));
    return b->second;
}

void Arch::addWire(IdString name, IdString type, int x, int y)
{
    NPNR_ASSERT(wires.count(name) == 0);
    WireInfo &wi = wires[name];
    wi.name = name;
    wi.type = type;
    wi.x = x;
    wi.y = y;

    wire_ids.push_back(name);
}

void Arch::addPip(IdString name, IdString type, IdString srcWire, IdString dstWire, DelayInfo delay, Loc loc)
{
    NPNR_ASSERT(pips.count(name) == 0);
    PipInfo &pi = pips[name];
    pi.name = name;
    pi.type = type;
    pi.srcWire = srcWire;
    pi.dstWire = dstWire;
    pi.delay = delay;
    pi.loc = loc;

    wire_info(srcWire).downhill.push_back(name);
    wire_info(dstWire).uphill.push_back(name);
    pip_ids.push_back(name);

    if (int(tilePipDimZ.size()) <= loc.x)
        tilePipDimZ.resize(loc.x + 1);

    if (int(tilePipDimZ[loc.x].size()) <= loc.y)
        tilePipDimZ[loc.x].resize(loc.y + 1);

    gridDimX = std::max(gridDimX, loc.x + 1);
    gridDimY = std::max(gridDimY, loc.x + 1);
    tilePipDimZ[loc.x][loc.y] = std::max(tilePipDimZ[loc.x][loc.y], loc.z + 1);
}

void Arch::addAlias(IdString name, IdString type, IdString srcWire, IdString dstWire, DelayInfo delay)
{
    NPNR_ASSERT(pips.count(name) == 0);
    PipInfo &pi = pips[name];
    pi.name = name;
    pi.type = type;
    pi.srcWire = srcWire;
    pi.dstWire = dstWire;
    pi.delay = delay;

    wire_info(srcWire).aliases.push_back(name);
    pip_ids.push_back(name);
}

void Arch::addBel(IdString name, IdString type, Loc loc, bool gb)
{
    NPNR_ASSERT(bels.count(name) == 0);
    NPNR_ASSERT(bel_by_loc.count(loc) == 0);
    BelInfo &bi = bels[name];
    bi.name = name;
    bi.type = type;
    bi.x = loc.x;
    bi.y = loc.y;
    bi.z = loc.z;
    bi.gb = gb;

    bel_ids.push_back(name);
    bel_by_loc[loc] = name;

    if (int(bels_by_tile.size()) <= loc.x)
        bels_by_tile.resize(loc.x + 1);

    if (int(bels_by_tile[loc.x].size()) <= loc.y)
        bels_by_tile[loc.x].resize(loc.y + 1);

    bels_by_tile[loc.x][loc.y].push_back(name);

    if (int(tileBelDimZ.size()) <= loc.x)
        tileBelDimZ.resize(loc.x + 1);

    if (int(tileBelDimZ[loc.x].size()) <= loc.y)
        tileBelDimZ[loc.x].resize(loc.y + 1);

    gridDimX = std::max(gridDimX, loc.x + 1);
    gridDimY = std::max(gridDimY, loc.x + 1);
    tileBelDimZ[loc.x][loc.y] = std::max(tileBelDimZ[loc.x][loc.y], loc.z + 1);
}

void Arch::addBelInput(IdString bel, IdString name, IdString wire)
{
    NPNR_ASSERT(bel_info(bel).pins.count(name) == 0);
    PinInfo &pi = bel_info(bel).pins[name];
    pi.name = name;
    pi.wire = wire;
    pi.type = PORT_IN;

    wire_info(wire).downhill_bel_pins.push_back(BelPin{bel, name});
    wire_info(wire).bel_pins.push_back(BelPin{bel, name});
}

void Arch::addBelOutput(IdString bel, IdString name, IdString wire)
{
    NPNR_ASSERT(bel_info(bel).pins.count(name) == 0);
    PinInfo &pi = bel_info(bel).pins[name];
    pi.name = name;
    pi.wire = wire;
    pi.type = PORT_OUT;

    wire_info(wire).uphill_bel_pin = BelPin{bel, name};
    wire_info(wire).bel_pins.push_back(BelPin{bel, name});
}

void Arch::addBelInout(IdString bel, IdString name, IdString wire)
{
    NPNR_ASSERT(bel_info(bel).pins.count(name) == 0);
    PinInfo &pi = bel_info(bel).pins[name];
    pi.name = name;
    pi.wire = wire;
    pi.type = PORT_INOUT;

    wire_info(wire).downhill_bel_pins.push_back(BelPin{bel, name});
    wire_info(wire).bel_pins.push_back(BelPin{bel, name});
}

void Arch::addGroupBel(IdString group, IdString bel) { groups[group].bels.push_back(bel); }

void Arch::addGroupWire(IdString group, IdString wire) { groups[group].wires.push_back(wire); }

void Arch::addGroupPip(IdString group, IdString pip) { groups[group].pips.push_back(pip); }

void Arch::addGroupGroup(IdString group, IdString grp) { groups[group].groups.push_back(grp); }

void Arch::addDecalGraphic(DecalId decal, const GraphicElement &graphic)
{
    decal_graphics[decal].push_back(graphic);
    refreshUi();
}

void Arch::setWireDecal(WireId wire, DecalXY decalxy)
{
    wire_info(wire).decalxy = decalxy;
    refreshUiWire(wire);
}

void Arch::setPipDecal(PipId pip, DecalXY decalxy)
{
    pip_info(pip).decalxy = decalxy;
    refreshUiPip(pip);
}

void Arch::setBelDecal(BelId bel, DecalXY decalxy)
{
    bel_info(bel).decalxy = decalxy;
    refreshUiBel(bel);
}

void Arch::setGroupDecal(GroupId group, DecalXY decalxy)
{
    groups[group].decalxy = decalxy;
    refreshUiGroup(group);
}

void Arch::setWireAttr(IdString wire, IdString key, const std::string &value) { wire_info(wire).attrs[key] = value; }

void Arch::setPipAttr(IdString pip, IdString key, const std::string &value) { pip_info(pip).attrs[key] = value; }

void Arch::setBelAttr(IdString bel, IdString key, const std::string &value) { bel_info(bel).attrs[key] = value; }

void Arch::setLutK(int K) { args.K = K; }

void Arch::setDelayScaling(double scale, double offset)
{
    args.delayScale = scale;
    args.delayOffset = offset;
}

void Arch::addCellTimingClock(IdString cell, IdString port) { cellTiming[cell].portClasses[port] = TMG_CLOCK_INPUT; }

void Arch::addCellTimingDelay(IdString cell, IdString fromPort, IdString toPort, DelayInfo delay)
{
    if (get_or_default(cellTiming[cell].portClasses, fromPort, TMG_IGNORE) == TMG_IGNORE)
        cellTiming[cell].portClasses[fromPort] = TMG_COMB_INPUT;
    if (get_or_default(cellTiming[cell].portClasses, toPort, TMG_IGNORE) == TMG_IGNORE)
        cellTiming[cell].portClasses[toPort] = TMG_COMB_OUTPUT;
    cellTiming[cell].combDelays[CellDelayKey{fromPort, toPort}] = delay;
}

void Arch::addCellTimingSetupHold(IdString cell, IdString port, IdString clock, DelayInfo setup, DelayInfo hold)
{
    TimingClockingInfo ci;
    ci.clock_port = clock;
    ci.edge = RISING_EDGE;
    ci.setup = setup;
    ci.hold = hold;
    cellTiming[cell].clockingInfo[port].push_back(ci);
    cellTiming[cell].portClasses[port] = TMG_REGISTER_INPUT;
}

void Arch::addCellTimingClockToOut(IdString cell, IdString port, IdString clock, DelayInfo clktoq)
{
    TimingClockingInfo ci;
    ci.clock_port = clock;
    ci.edge = RISING_EDGE;
    ci.clockToQ = clktoq;
    cellTiming[cell].clockingInfo[port].push_back(ci);
    cellTiming[cell].portClasses[port] = TMG_REGISTER_OUTPUT;
}

// ---------------------------------------------------------------

void Arch::setupPipsForCLB(int x, int y, int numSingleWires, int numDoubleWires, int side) {

    std::string CBType = "";
    int xWire = x;
    int yWire = y;
    int lut1Id = 0;
    int lut0Id = 1;
    int dff1Id = 0;
    int dff0Id = 1;

    if (side == 0) {
        // East
        CBType = "CB0";
        xWire = x;
        yWire = y;
        lut1Id = 0;
        lut0Id = 1;
        dff1Id = 0 + 8;
        dff0Id = 1 + 8;
    } else if (side == 1) {
        // North
        CBType = "CB1";
        xWire = x;
        yWire = y;
        lut1Id = 2;
        lut0Id = 3;
        dff1Id = 2 + 8;
        dff0Id = 3 + 8;
    } else if (side == 2) {
        // West
        CBType = "CB0";
        xWire = x - 1;
        yWire = y;
        lut1Id = 4;
        lut0Id = 5;
        dff1Id = 4 + 8;
        dff0Id = 5 + 8;
    } else if (side == 3) {
        // South
        CBType = "CB1";
        xWire = x;
        yWire = y - 1;
        lut1Id = 6;
        lut0Id = 7;
        dff1Id = 6 + 8;
        dff0Id = 7 + 8;
    }

    NPNR_ASSERT(side < 4);

    if (xWire < 0 || yWire < 0)
        return;

    // PipInfo
    IdString pipType = id("PIP");

    // CLB inputs <- ConnectionBlock
    for (int k = 0; k < 4; k++) {
        std::string lut1_input_name = "X" + std::to_string(x) +
                                      "Y" + std::to_string(y) +
                                      "Z" + std::to_string(lut1Id) +
                                      "_" + "I" + std::to_string(k);
        WireId dst0Wire = getWireByName(id(lut1_input_name));
        std::string lut0_input_name = "X" + std::to_string(x) +
                                      "Y" + std::to_string(y) +
                                      "Z" + std::to_string(lut0Id) +
                                      "_" + "I" + std::to_string(k);
        WireId dst1Wire = getWireByName(id(lut0_input_name));

        for (int s = 0; s < numSingleWires; s++) {
            std::string cb_single_name = "X" + std::to_string(xWire) +
                                         "Y" + std::to_string(yWire) + "_" +
                                         CBType + "_SINGLE" + std::to_string(s);
            WireId srcWire = getWireByName(id(cb_single_name));
            std::string pip0Name = "X" + std::to_string(x) +
                                   "Y" + std::to_string(y) + "_" +
                                   CBType + "_SINGLE" + std::to_string(s) +
                                   "->LUT" + std::to_string(lut1Id) + "_I" +
                                   std::to_string(k);
            std::string pip1Name = "X" + std::to_string(x) +
                                   "Y" + std::to_string(y) + "_" +
                                   CBType + "_SINGLE" + std::to_string(s) +
                                   "->LUT" + std::to_string(lut0Id) + "_I" +
                                   std::to_string(k);
            addPip(id(pip0Name), pipType, getWireName(srcWire), getWireName(dst0Wire), getDelayFromNS(0.01), Loc(x, y, 0));
            addPip(id(pip1Name), pipType, getWireName(srcWire), getWireName(dst1Wire), getDelayFromNS(0.01), Loc(x, y, 0));
        }

        for (int s = 0; s < numDoubleWires; s++) {
            std::string cb_double_name = "X" + std::to_string(xWire) +
                                         "Y" + std::to_string(yWire) + "_" +
                                         CBType + "_DOUBLE" + std::to_string(s);
            WireId srcWire = getWireByName(id(cb_double_name));
            std::string pip0Name = "X" + std::to_string(x) +
                                   "Y" + std::to_string(y) + "_" +
                                   CBType + "_DOUBLE" + std::to_string(s) +
                                   "->LUT" + std::to_string(lut1Id) + "_I" +
                                   std::to_string(k);
            std::string pip1Name = "X" + std::to_string(x) +
                                   "Y" + std::to_string(y) + "_" +
                                   CBType + "_DOUBLE" + std::to_string(s) +
                                   "->LUT" + std::to_string(lut0Id) + "_I" +
                                   std::to_string(k);
            addPip(id(pip0Name), pipType, getWireName(srcWire), getWireName(dst0Wire), getDelayFromNS(0.01), Loc(x, y, 0));
            addPip(id(pip1Name), pipType, getWireName(srcWire), getWireName(dst1Wire), getDelayFromNS(0.01), Loc(x, y, 0));
        }
    }

    // CLB outputs -> ConnectionBlock
    std::string lut1_output_name = "X" + std::to_string(x) +
                                   "Y" + std::to_string(y) +
                                   "Z" + std::to_string(lut1Id) + "_" + "O";
    WireId src0LutWire = getWireByName(id(lut1_output_name));

    std::string lut0_output_name = "X" + std::to_string(x) +
                                   "Y" + std::to_string(y) +
                                   "Z" + std::to_string(lut0Id) + "_" + "O";
    WireId src1LutWire = getWireByName(id(lut0_output_name));

    std::string dff1_output_name = "X" + std::to_string(x) +
                                   "Y" + std::to_string(y) +
                                   "Z" + std::to_string(dff1Id) + "_" + "Q";
    WireId src0DffWire = getWireByName(id(dff1_output_name));

    std::string dff0_output_name = "X" + std::to_string(x) +
                                   "Y" + std::to_string(y) +
                                   "Z" + std::to_string(dff0Id) + "_" + "Q";
    WireId src1DffWire = getWireByName(id(dff0_output_name));

    for (int s = 0; s < numSingleWires; s++) {
        std::string cb_single_name = "X" + std::to_string(x) +
                                     "Y" + std::to_string(y) +
                                     "_" + CBType + "_SINGLE" + std::to_string(s);
        WireId dstWire = getWireByName(id(cb_single_name));
        std::string pip0LutName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "_LUT" + std::to_string(lut1Id) +"_O" +
                                  "->" + CBType + "_SINGLE" + std::to_string(s);
        std::string pip1LutName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "_LUT" + std::to_string(lut0Id) +"_O" +
                                  "->" + CBType + "_SINGLE" + std::to_string(s);
        std::string pip0DffName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "_DFF" + std::to_string(dff1Id) +"_O" +
                                  "->" + CBType + "_SINGLE" + std::to_string(s);
        std::string pip1DffName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "_DFF" + std::to_string(dff0Id) +"_O" +
                                  "->" + CBType + "_SINGLE" + std::to_string(s);

        addPip(id(pip0LutName), pipType, getWireName(src0LutWire), getWireName(dstWire), getDelayFromNS(0.01), Loc(x, y, 0));
        addPip(id(pip1LutName), pipType, getWireName(src1LutWire), getWireName(dstWire), getDelayFromNS(0.01), Loc(x, y, 0));
        addPip(id(pip0DffName), pipType, getWireName(src0DffWire), getWireName(dstWire), getDelayFromNS(0.01), Loc(x, y, 0));
        addPip(id(pip1DffName), pipType, getWireName(src1DffWire), getWireName(dstWire), getDelayFromNS(0.01), Loc(x, y, 0));
    }

    for (int s = 0; s < numDoubleWires / 2; s++) {
        std::string cb_double_name = "X" + std::to_string(x) +
                                     "Y" + std::to_string(y) +
                                     "_" + CBType + "_DOUBLE" + std::to_string(s);
        WireId dstWire = getWireByName(id(cb_double_name));
        std::string pip0LutName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "_LUT" + std::to_string(lut1Id) +"_O" +
                                  "->" + CBType + "_DOUBLE" + std::to_string(s);
        std::string pip1LutName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "_LUT" + std::to_string(lut0Id) +"_O" +
                                  "->" + CBType + "_DOUBLE" + std::to_string(s);
        std::string pip0DffName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "_DFF" + std::to_string(dff1Id) +"_O" +
                                  "->" + CBType + "_DOUBLE" + std::to_string(s);
        std::string pip1DffName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "_DFF" + std::to_string(dff0Id) +"_O" +
                                  "->" + CBType + "_DOUBLE" + std::to_string(s);

        addPip(id(pip0LutName), pipType, getWireName(src0LutWire), getWireName(dstWire), getDelayFromNS(0.01), Loc(x, y, 0));
        addPip(id(pip1LutName), pipType, getWireName(src1LutWire), getWireName(dstWire), getDelayFromNS(0.01), Loc(x, y, 0));
        addPip(id(pip0DffName), pipType, getWireName(src0DffWire), getWireName(dstWire), getDelayFromNS(0.01), Loc(x, y, 0));
        addPip(id(pip1DffName), pipType, getWireName(src1DffWire), getWireName(dstWire), getDelayFromNS(0.01), Loc(x, y, 0));
    }
}

Arch::Arch(ArchArgs args) : chipName("borca"), args(args)
{
    // Dummy for empty decals
    decal_graphics[IdString()];
    gridDimX = 8;
    gridDimY = 8;

    // Number of cells per Tile
    // 8 4-LUTs, 8 FFs, 1 CARRY4, 3 MUXES
    int numBelsPerTile = 8 + 8 + 1 + 3;
    // Number of PIPs per Tile
    // 2 x CB + SB
    int numPipsPerTile = 415 * 2 + 48;

    for (int x = 0; x < gridDimX; x++) {
      std::vector<int> tmp0, tmp1;
      for (int y = 0; y < gridDimY; y++) {
        tmp0.push_back(numBelsPerTile);
        tmp1.push_back(numPipsPerTile);
      }
      tileBelDimZ.push_back(tmp0);
      tilePipDimZ.push_back(tmp1);
    }

    // WireInfo & BelInfo
    IdString belInputType  = id("BEL_INPUT");
    IdString belOutputType = id("BEL_OUTPUT");

    for (int x = 0; x < gridDimX; x++) {
      for (int y = 0; y < gridDimY; y++) {
        for (int z = 0; z < numBelsPerTile; z++) {
          if (z >= 0 && z < 8) {
            // LUTs
            std::string i0Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "I0";
            std::string i1Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "I1";
            std::string i2Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "I2";
            std::string i3Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "I3";
            std::string outName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "Z" + std::to_string(z) + "_" + "O";
            addWire(id(i0Name),  belInputType, x, y);
            addWire(id(i1Name),  belInputType, x, y);
            addWire(id(i2Name),  belInputType, x, y);
            addWire(id(i3Name),  belInputType, x, y);
            addWire(id(outName), belOutputType, x, y);

            std::string belName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "Z" + std::to_string(z) + "_" + "LUT4_BEL";
            IdString belId = id(belName);
            IdString belType = id("LUT4");
            addBel(belId, belType, Loc(x, y, z), false);
            addBelInput(belId, id("I0"), id(i0Name));
            addBelInput(belId, id("I1"), id(i1Name));
            addBelInput(belId, id("I2"), id(i2Name));
            addBelInput(belId, id("I3"), id(i3Name));
            addBelOutput(belId, id("O"), id(outName));
          } else if (z >= 8 && z < 16) {
            // DFFERs
            std::string dName = "X" + std::to_string(x) +
                                "Y" + std::to_string(y) +
                                "Z" + std::to_string(z) + "_" + "D";
            std::string qName = "X" + std::to_string(x) +
                                "Y" + std::to_string(y) +
                                "Z" + std::to_string(z) + "_" + "Q";
            std::string clkName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "Z" + std::to_string(z) + "_" + "CLK";
            std::string ceName = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "CE";
            std::string rstName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "Z" + std::to_string(z) + "_" + "RST";
            addWire(id(dName),   belInputType, x, y);
            addWire(id(clkName), belInputType, x, y);
            addWire(id(ceName),  belInputType, x, y);
            addWire(id(rstName), belInputType, x, y);
            addWire(id(qName),   belOutputType, x, y);

            std::string belName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "Z" + std::to_string(z) + "_" + "DFFER_BEL";
            IdString belId = id(belName);
            IdString belType = id("DFFER");
            addBel(belId, belType, Loc(x, y, z), false);
            addBelInput(belId, id("D"), id(dName));
            addBelInput(belId, id("CLK"), id(clkName));
            addBelInput(belId, id("CE"), id(ceName));
            addBelInput(belId, id("RST"), id(rstName));
            addBelOutput(belId, id("Q"), id(qName));
          } else if (z >= 16 && z < 19) {
            // MUXes
            std::string i0Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "I0";
            std::string i1Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "I1";
            std::string selName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "Z" + std::to_string(z) + "_" + "SEL";
            std::string outName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "Z" + std::to_string(z) + "_" + "O";
            addWire(id(i0Name), belInputType, x, y);
            addWire(id(i1Name), belInputType, x, y);
            addWire(id(selName), belInputType, x, y);
            addWire(id(outName), belOutputType, x, y);

            std::string belName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "Z" + std::to_string(z) + "_" + "MUX_BEL";
            IdString belId = id(belName);
            IdString belType = id("MUX");
            addBel(belId, belType, Loc(x, y, z), false);
            addBelInput(belId, id("I0"), id(i0Name));
            addBelInput(belId, id("I1"), id(i1Name));
            addBelOutput(belId, id("O"), id(outName));
          } else {
            // CARRY
            std::string ciName = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "CI";
            std::string coName = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "CO";

            std::string g0Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "G[0]";
            std::string g1Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "G[1]";
            std::string g2Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "G[2]";
            std::string g3Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "G[3]";

            std::string p0Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "P[0]";
            std::string p1Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "P[1]";
            std::string p2Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "P[2]";
            std::string p3Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "P[3]";

            std::string s0Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "S[0]";
            std::string s1Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "S[1]";
            std::string s2Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "S[2]";
            std::string s3Name = "X" + std::to_string(x) +
                                 "Y" + std::to_string(y) +
                                 "Z" + std::to_string(z) + "_" + "S[3]";
            addWire(id(ciName), belInputType, x, y);
            addWire(id(coName), belOutputType, x, y);

            addWire(id(g0Name), belInputType, x, y);
            addWire(id(g1Name), belInputType, x, y);
            addWire(id(g2Name), belInputType, x, y);
            addWire(id(g3Name), belInputType, x, y);

            addWire(id(p0Name), belInputType, x, y);
            addWire(id(p1Name), belInputType, x, y);
            addWire(id(p2Name), belInputType, x, y);
            addWire(id(p3Name), belInputType, x, y);

            addWire(id(s0Name), belOutputType, x, y);
            addWire(id(s1Name), belOutputType, x, y);
            addWire(id(s2Name), belOutputType, x, y);
            addWire(id(s3Name), belOutputType, x, y);

            std::string belName = "X" + std::to_string(x) +
                                  "Y" + std::to_string(y) +
                                  "Z" + std::to_string(z) + "_" + "CARRY4_BEL";
            IdString belId = id(belName);
            IdString belType = id("CARRY4");
            addBel(belId, belType, Loc(x, y, z), false);
            addBelInput(belId, id("CI"), id(ciName));
            addBelOutput(belId, id("CO"), id(coName));

            addBelInput(belId, id("G[0]"), id(g0Name));
            addBelInput(belId, id("G[1]"), id(g1Name));
            addBelInput(belId, id("G[2]"), id(g2Name));
            addBelInput(belId, id("G[3]"), id(g3Name));

            addBelInput(belId, id("P[0]"), id(p0Name));
            addBelInput(belId, id("P[1]"), id(p1Name));
            addBelInput(belId, id("P[2]"), id(p2Name));
            addBelInput(belId, id("P[3]"), id(p3Name));

            addBelOutput(belId, id("S[0]"), id(s0Name));
            addBelOutput(belId, id("S[1]"), id(s1Name));
            addBelOutput(belId, id("S[2]"), id(s2Name));
            addBelOutput(belId, id("S[3]"), id(s3Name));
          }
        }
      }
    }

    int numSingleWires = 4;
    int numDoubleWires = 8;
    IdString cbWireType = id("CB_WIRE");
    IdString sbWireType = id("SB_WIRE");

    for (int x = 0; x < gridDimX; x++) {
      for (int y = 0; y < gridDimY; y++) {
        for (int k = 0; k < numSingleWires; k++) {
          std::string cb0_single_name = "X" + std::to_string(x) +
                                        "Y" + std::to_string(y) + "_CB0_SINGLE" + std::to_string(k);
          std::string cb1_single_name = "X" + std::to_string(x) +
                                        "Y" + std::to_string(y) + "_CB1_SINGLE" + std::to_string(k);
          std::string sb0_single_name = "X" + std::to_string(x) +
                                        "Y" + std::to_string(y) + "_SB0_SINGLE" + std::to_string(k);
          std::string sb1_single_name = "X" + std::to_string(x) +
                                        "Y" + std::to_string(y) + "_SB1_SINGLE" + std::to_string(k);

          addWire(id(cb0_single_name), cbWireType, x, y);
          addWire(id(cb1_single_name), cbWireType, x, y);
          addWire(id(sb0_single_name), sbWireType, x, y);
          addWire(id(sb1_single_name), sbWireType, x, y);
        }

        for (int k = 0; k < numDoubleWires; k++) {
          std::string cb0_double_name = "X" + std::to_string(x) +
                                        "Y" + std::to_string(y) + "_CB0_DOUBLE" + std::to_string(k);
          std::string cb1_double_name = "X" + std::to_string(x) +
                                        "Y" + std::to_string(y) + "_CB1_DOUBLE" + std::to_string(k);
          std::string sb0_double_name = "X" + std::to_string(x) +
                                        "Y" + std::to_string(y) + "_SB0_DOUBLE" + std::to_string(k);
          std::string sb1_double_name = "X" + std::to_string(x) +
                                        "Y" + std::to_string(y) + "_SB1_DOUBLE" + std::to_string(k);

          addWire(id(cb0_double_name), cbWireType, x, y);
          addWire(id(cb1_double_name), cbWireType, x, y);
          addWire(id(sb0_double_name), sbWireType, x, y);
          addWire(id(sb1_double_name), sbWireType, x, y);
        }
      }
    }

    // PipInfo
    IdString pipType = id("PIP");

    // East:  LUT0 (1) ->LUT1 (0)
    // South: LUT2 (3) ->LUT3 (2)
    // West:  LUT4 (5) ->LUT5 (4)
    // North: LUT6 (7) ->LUT7 (6)
    for (int x = 0; x < gridDimX; x++) {
        for (int y = 0; y < gridDimY; y++) {
            setupPipsForCLB(x, y, numSingleWires, numDoubleWires, 0); // East CLB IOs
            setupPipsForCLB(x, y, numSingleWires, numDoubleWires, 1); // North CLB IOs
            setupPipsForCLB(x, y, numSingleWires, numDoubleWires, 2); // West CLB IOs
            setupPipsForCLB(x, y, numSingleWires, numDoubleWires, 3); // South CLB IOs
        }
    }

    // Intra-CLB connections
    // LUT (O) -> CARRY (P/G)
    // LUT (O) -> DFF (D)
    // CARRY (S) -> DFF (D)
    // LUT (O) -> CLB_COMB_O
    // CARRY (S) -> CLB_COMB_O
    // DFF (Q) -> CLB_SYNC_O
     for (int x = 0; x < gridDimX; x++) {
        for (int y = 0; y < gridDimY; y++) {
            // TODO
        }
    }

    // SB {N, S, E, W}
    // switchbox-element-two mapping
    // N0E0, N0S0, N0W1, N1E1, N1S1, N1W0
    // E0W0, E0N0, E0S1, E1W1, E1N1, E1S0
    // S0W0, S0N0, S0E1, S1W1, S1N1, S1E0
    // W0E0, W0S0, W0N1, W1E1, W1S1, W1N0
    for (int x = 0; x < gridDimX; x++) {
        for (int y = 0; y < gridDimY; y++) {
            // TODO
        }
    }

    // CB <-> SB
    for (int x = 0; x < gridDimX; x++) {
        for (int y = 0; y < gridDimY; y++) {
            bool isNorthAvail = (y + 1) < gridDimY;
            bool isEastAvail  = (x + 1) < gridDimX;
            bool isSouthAvail = (x - 1) >= 0;
            bool isWestAvail  = (y - 1) >= 0;

            // FIXME: how to capture bidir property of single wires and double wires?
            // For now, use separate PIP for each direction

            // CBs and SB in the same tile
            for (int s = 0; s < numSingleWires; s++) {
                std::string cb0WireName = "X" + std::to_string(x) +
                                          "Y" + std::to_string(y) +
                                          "_CB0_SINGLE" + std::to_string(s);
                std::string cb1WireName = "X" + std::to_string(x) +
                                          "Y" + std::to_string(y) +
                                          "_CB1_SINGLE" + std::to_string(s);
                std::string sbWireName = "X" + std::to_string(x) +
                                         "Y" + std::to_string(y) +
                                         "_SB_SINGLE" + std::to_string(s);

                WireId cb0Wire = getWireByName(id(cb0WireName));
                WireId cb1Wire = getWireByName(id(cb1WireName));
                WireId sbWire  = getWireByName(id(sbWireName));

                // CB->SB
                std::string pip0Name = "X" + std::to_string(x) +
                                       "Y" + std::to_string(y) +
                                       "_CB0->SB_SINGLE" + std::to_string(s);
                std::string pip1Name = "X" + std::to_string(x) +
                                       "Y" + std::to_string(y) +
                                       "_CB1->SB_SINGLE" + std::to_string(s);
                // SB->CB
                std::string pip2Name = "X" + std::to_string(x) +
                                       "Y" + std::to_string(y) +
                                       "_SB->CB0_SINGLE" + std::to_string(s);
                std::string pip3Name = "X" + std::to_string(x) +
                                       "Y" + std::to_string(y) +
                                       "_SB->CB1_SINGLE" + std::to_string(s);

                addPip(id(pip0Name), pipType,
                       getWireName(cb0Wire), getWireName(sbWire),
                       getDelayFromNS(0.01), Loc(x, y, 0));
                addPip(id(pip1Name), pipType,
                       getWireName(cb1Wire), getWireName(sbWire),
                       getDelayFromNS(0.01), Loc(x, y, 0));

                addPip(id(pip2Name), pipType,
                       getWireName(sbWire), getWireName(cb0Wire),
                       getDelayFromNS(0.01), Loc(x, y, 0));
                addPip(id(pip3Name), pipType,
                       getWireName(sbWire), getWireName(cb1Wire),
                       getDelayFromNS(0.01), Loc(x, y, 0));
            }

            // The second half of the double wires connect to the SB of the same tile
            for (int s = 4; s < numDoubleWires; s++) {
                std::string cb0WireName = "X" + std::to_string(x) +
                                          "Y" + std::to_string(y) +
                                          "_CB0_DOUBLE" + std::to_string(s);
                std::string cb1WireName = "X" + std::to_string(x) +
                                          "Y" + std::to_string(y) +
                                          "_CB1_DOUBLE" + std::to_string(s);
                std::string sbWireName = "X" + std::to_string(x) +
                                         "Y" + std::to_string(y) +
                                         "_SB_DOUBLE" + std::to_string(s);

                WireId cb0Wire = getWireByName(id(cb0WireName));
                WireId cb1Wire = getWireByName(id(cb1WireName));
                WireId sbWire  = getWireByName(id(sbWireName));

                // CB->SB
                std::string pip0Name = "X" + std::to_string(x) +
                                       "Y" + std::to_string(y) +
                                       "_CB0->SB_DOUBLE" + std::to_string(s);
                std::string pip1Name = "X" + std::to_string(x) +
                                       "Y" + std::to_string(y) +
                                       "_CB1->SB_DOUBLE" + std::to_string(s);
                // SB->CB
                std::string pip2Name = "X" + std::to_string(x) +
                                       "Y" + std::to_string(y) +
                                       "_SB->CB0_DOUBLE" + std::to_string(s);
                std::string pip3Name = "X" + std::to_string(x) +
                                       "Y" + std::to_string(y) +
                                       "_SB->CB1_DOUBLE" + std::to_string(s);

                addPip(id(pip0Name), pipType,
                       getWireName(cb0Wire), getWireName(sbWire),
                       getDelayFromNS(0.01), Loc(x, y, 0));
                addPip(id(pip1Name), pipType,
                       getWireName(cb1Wire), getWireName(sbWire),
                       getDelayFromNS(0.01), Loc(x, y, 0));

                addPip(id(pip2Name), pipType,
                       getWireName(sbWire), getWireName(cb0Wire),
                       getDelayFromNS(0.01), Loc(x, y, 0));
                addPip(id(pip3Name), pipType,
                       getWireName(sbWire), getWireName(cb1Wire),
                       getDelayFromNS(0.01), Loc(x, y, 0));
            }

            // CB0 and SB from the South tile
            if (isSouthAvail) {
                for (int s = 0; s < numSingleWires; s++) {
                    std::string cb0WireName = "X" + std::to_string(x) +
                                              "Y" + std::to_string(y) +
                                              "_CB0_SINGLE" + std::to_string(s);
                    std::string sbWireName = "X" + std::to_string(x - 1) +
                                             "Y" + std::to_string(y) +
                                             "_SB_SINGLE" + std::to_string(s);

                    WireId cb0Wire = getWireByName(id(cb0WireName));
                    WireId sbWire  = getWireByName(id(sbWireName));

                    // CB->SB
                    std::string pipCB2SBName = "X" + std::to_string(x) +
                                               "Y" + std::to_string(y) +
                                               "_CB0->SB_SINGLE" + std::to_string(s);
                    // SB->CB
                    std::string pipSB2CBName = "X" + std::to_string(x - 1) +
                                               "Y" + std::to_string(y) +
                                               "_SB->CB0_SINGLE" + std::to_string(s);

                    addPip(id(pipCB2SBName), pipType,
                           getWireName(cb0Wire), getWireName(sbWire),
                           getDelayFromNS(0.01), Loc(x, y, 0));
                    addPip(id(pipSB2CBName), pipType,
                           getWireName(sbWire), getWireName(cb0Wire),
                           getDelayFromNS(0.01), Loc(x - 1, y, 0));
                }

                for (int s = 4; s < numDoubleWires; s++) {
                    std::string cb0WireName = "X" + std::to_string(x) +
                                              "Y" + std::to_string(y) +
                                              "_CB0_DOUBLE" + std::to_string(s);
                    std::string sbWireName = "X" + std::to_string(x - 1) +
                                             "Y" + std::to_string(y) +
                                             "_SB_DOUBLE" + std::to_string(s);

                    WireId cb0Wire = getWireByName(id(cb0WireName));
                    WireId sbWire  = getWireByName(id(sbWireName));

                    // CB->SB
                    std::string pipCB2SBName = "X" + std::to_string(x) +
                                               "Y" + std::to_string(y) +
                                               "_CB0->SB_DOUBLE" + std::to_string(s);
                    // SB->CB
                    std::string pipSB2CBName = "X" + std::to_string(x - 1) +
                                               "Y" + std::to_string(y) +
                                               "_SB->CB0_DOUBLE" + std::to_string(s);

                    addPip(id(pipCB2SBName), pipType,
                           getWireName(cb0Wire), getWireName(sbWire),
                           getDelayFromNS(0.01), Loc(x, y, 0));
                    addPip(id(pipSB2CBName), pipType,
                           getWireName(sbWire), getWireName(cb0Wire),
                           getDelayFromNS(0.01), Loc(x - 1, y, 0));
                }
            }

            // CB1 and SB from the West tile
            if (isWestAvail) {
                for (int s = 0; s < numSingleWires; s++) {
                    std::string cb1WireName = "X" + std::to_string(x) +
                                              "Y" + std::to_string(y) +
                                              "_CB1_SINGLE" + std::to_string(s);
                    std::string sbWireName = "X" + std::to_string(x) +
                                             "Y" + std::to_string(y - 1) +
                                             "_SB_SINGLE" + std::to_string(s);

                    WireId cb1Wire = getWireByName(id(cb1WireName));
                    WireId sbWire  = getWireByName(id(sbWireName));

                    // CB->SB
                    std::string pipCB2SBName = "X" + std::to_string(x) +
                                               "Y" + std::to_string(y) +
                                               "_CB1->SB_SINGLE" + std::to_string(s);
                    // SB->CB
                    std::string pipSB2CBName = "X" + std::to_string(x) +
                                               "Y" + std::to_string(y - 1) +
                                               "_SB->CB1_SINGLE" + std::to_string(s);

                    addPip(id(pipCB2SBName), pipType,
                           getWireName(cb1Wire), getWireName(sbWire),
                           getDelayFromNS(0.01), Loc(x, y, 0));
                    addPip(id(pipSB2CBName), pipType,
                           getWireName(sbWire), getWireName(cb1Wire),
                           getDelayFromNS(0.01), Loc(x - 1, y, 0));
                }

                for (int s = 4; s < numDoubleWires; s++) {
                    std::string cb1WireName = "X" + std::to_string(x) +
                                              "Y" + std::to_string(y) +
                                              "_CB1_DOUBLE" + std::to_string(s);
                    std::string sbWireName = "X" + std::to_string(x) +
                                             "Y" + std::to_string(y - 1) +
                                             "_SB_DOUBLE" + std::to_string(s);

                    WireId cb1Wire = getWireByName(id(cb1WireName));
                    WireId sbWire  = getWireByName(id(sbWireName));

                    // CB->SB
                    std::string pipCB2SBName = "X" + std::to_string(x) +
                                               "Y" + std::to_string(y) +
                                               "_CB1->SB_DOUBLE" + std::to_string(s);
                    // SB->CB
                    std::string pipSB2CBName = "X" + std::to_string(x) +
                                               "Y" + std::to_string(y - 1) +
                                               "_SB->CB1_DOUBLE" + std::to_string(s);

                    addPip(id(pipCB2SBName), pipType,
                           getWireName(cb1Wire), getWireName(sbWire),
                           getDelayFromNS(0.01), Loc(x, y, 0));
                    addPip(id(pipSB2CBName), pipType,
                           getWireName(sbWire), getWireName(cb1Wire),
                           getDelayFromNS(0.01), Loc(x - 1, y, 0));
                }
            }

            // The first-half of the double wires connect to the SB of the adjacent tile
            // (skipping the SB of the same tile)
            // CB0 and SB from the North tile
            // CB1 and SB from the  East tile
            for (int s = 0; s < numDoubleWires / 2; s++) {
                if (isNorthAvail) {
                    std::string cb0WireName = "X" + std::to_string(x) +
                                              "Y" + std::to_string(y) +
                                              "_CB0_DOUBLE" + std::to_string(s);
                    std::string sbWireName  = "X" + std::to_string(x) +
                                              "Y" + std::to_string(y + 1) +
                                              "_SB_DOUBLE" + std::to_string(s);
                    WireId cb0Wire = getWireByName(id(cb0WireName));
                    WireId sbWire  = getWireByName(id(sbWireName));

                    // CB->SB
                    std::string pipCB2SBName = "X" + std::to_string(x) +
                                               "Y" + std::to_string(y) +
                                               "_CB0->SB_DOUBLE" + std::to_string(s);
                    // SB->CB
                    std::string pipSB2CBName = "X" + std::to_string(x) +
                                               "Y" + std::to_string(y + 1) +
                                               "_SB->CB0_DOUBLE" + std::to_string(s);
                    addPip(id(pipCB2SBName), pipType,
                           getWireName(cb0Wire), getWireName(sbWire),
                           getDelayFromNS(0.01), Loc(x, y, 0));
                    addPip(id(pipSB2CBName), pipType,
                           getWireName(sbWire), getWireName(cb0Wire),
                           getDelayFromNS(0.01), Loc(x, y + 1, 0));
                }

                if (isEastAvail) {
                    std::string cb1WireName = "X" + std::to_string(x) +
                                              "Y" + std::to_string(y) +
                                              "_CB1_DOUBLE" + std::to_string(s);
                    std::string sbWireName  = "X" + std::to_string(x + 1) +
                                              "Y" + std::to_string(y) +
                                              "_SB_DOUBLE" + std::to_string(s);
                    WireId cb1Wire = getWireByName(id(cb1WireName));
                    WireId sbWire  = getWireByName(id(sbWireName));

                    // CB->SB
                    std::string pipCB2SBName = "X" + std::to_string(x) +
                                               "Y" + std::to_string(y) +
                                               "_CB0->SB_DOUBLE" + std::to_string(s);
                    // SB->CB
                    std::string pipSB2CBName = "X" + std::to_string(x + 1) +
                                               "Y" + std::to_string(y) +
                                               "_SB->CB0_DOUBLE" + std::to_string(s);
                    addPip(id(pipCB2SBName), pipType,
                           getWireName(cb1Wire), getWireName(sbWire),
                           getDelayFromNS(0.01), Loc(x, y, 0));
                    addPip(id(pipSB2CBName), pipType,
                           getWireName(sbWire), getWireName(cb1Wire),
                           getDelayFromNS(0.01), Loc(x + 1, y, 0));
                }
            }
        }
    }
}

void IdString::initialize_arch(const BaseCtx *ctx) {}

// ---------------------------------------------------------------

BelId Arch::getBelByName(IdString name) const
{
    if (bels.count(name))
        return name;
    return BelId();
}

IdString Arch::getBelName(BelId bel) const { return bel; }

Loc Arch::getBelLocation(BelId bel) const
{
    auto &info = bels.at(bel);
    return Loc(info.x, info.y, info.z);
}

BelId Arch::getBelByLocation(Loc loc) const
{
    auto it = bel_by_loc.find(loc);
    if (it != bel_by_loc.end())
        return it->second;
    return BelId();
}

const std::vector<BelId> &Arch::getBelsByTile(int x, int y) const { return bels_by_tile.at(x).at(y); }

bool Arch::getBelGlobalBuf(BelId bel) const { return bels.at(bel).gb; }

uint32_t Arch::getBelChecksum(BelId bel) const
{
    // FIXME
    return 0;
}

void Arch::bindBel(BelId bel, CellInfo *cell, PlaceStrength strength)
{
    bels.at(bel).bound_cell = cell;
    cell->bel = bel;
    cell->belStrength = strength;
    refreshUiBel(bel);
}

void Arch::unbindBel(BelId bel)
{
    bels.at(bel).bound_cell->bel = BelId();
    bels.at(bel).bound_cell->belStrength = STRENGTH_NONE;
    bels.at(bel).bound_cell = nullptr;
    refreshUiBel(bel);
}

bool Arch::checkBelAvail(BelId bel) const { return bels.at(bel).bound_cell == nullptr; }

CellInfo *Arch::getBoundBelCell(BelId bel) const { return bels.at(bel).bound_cell; }

CellInfo *Arch::getConflictingBelCell(BelId bel) const { return bels.at(bel).bound_cell; }

const std::vector<BelId> &Arch::getBels() const { return bel_ids; }

IdString Arch::getBelType(BelId bel) const { return bels.at(bel).type; }

const std::map<IdString, std::string> &Arch::getBelAttrs(BelId bel) const { return bels.at(bel).attrs; }

WireId Arch::getBelPinWire(BelId bel, IdString pin) const
{
    const auto &bdata = bels.at(bel);
    if (!bdata.pins.count(pin))
        log_error("bel '%s' has no pin '%s'\n", bel.c_str(this), pin.c_str(this));
    return bdata.pins.at(pin).wire;
}

PortType Arch::getBelPinType(BelId bel, IdString pin) const { return bels.at(bel).pins.at(pin).type; }

std::vector<IdString> Arch::getBelPins(BelId bel) const
{
    std::vector<IdString> ret;
    for (auto &it : bels.at(bel).pins)
        ret.push_back(it.first);
    return ret;
}

// ---------------------------------------------------------------

WireId Arch::getWireByName(IdString name) const
{
    if (wires.count(name))
        return name;
    return WireId();
}

IdString Arch::getWireName(WireId wire) const { return wire; }

IdString Arch::getWireType(WireId wire) const { return wires.at(wire).type; }

const std::map<IdString, std::string> &Arch::getWireAttrs(WireId wire) const { return wires.at(wire).attrs; }

uint32_t Arch::getWireChecksum(WireId wire) const
{
    // FIXME
    return 0;
}

void Arch::bindWire(WireId wire, NetInfo *net, PlaceStrength strength)
{
    wires.at(wire).bound_net = net;
    net->wires[wire].pip = PipId();
    net->wires[wire].strength = strength;
    refreshUiWire(wire);
}

void Arch::unbindWire(WireId wire)
{
    auto &net_wires = wires.at(wire).bound_net->wires;

    auto pip = net_wires.at(wire).pip;
    if (pip != PipId()) {
        pips.at(pip).bound_net = nullptr;
        refreshUiPip(pip);
    }

    net_wires.erase(wire);
    wires.at(wire).bound_net = nullptr;
    refreshUiWire(wire);
}

bool Arch::checkWireAvail(WireId wire) const { return wires.at(wire).bound_net == nullptr; }

NetInfo *Arch::getBoundWireNet(WireId wire) const { return wires.at(wire).bound_net; }

NetInfo *Arch::getConflictingWireNet(WireId wire) const { return wires.at(wire).bound_net; }

const std::vector<BelPin> &Arch::getWireBelPins(WireId wire) const { return wires.at(wire).bel_pins; }

const std::vector<WireId> &Arch::getWires() const { return wire_ids; }

// ---------------------------------------------------------------

PipId Arch::getPipByName(IdString name) const
{
    if (pips.count(name))
        return name;
    return PipId();
}

IdString Arch::getPipName(PipId pip) const { return pip; }

IdString Arch::getPipType(PipId pip) const { return pips.at(pip).type; }

const std::map<IdString, std::string> &Arch::getPipAttrs(PipId pip) const { return pips.at(pip).attrs; }

uint32_t Arch::getPipChecksum(PipId wire) const
{
    // FIXME
    return 0;
}

void Arch::bindPip(PipId pip, NetInfo *net, PlaceStrength strength)
{
    WireId wire = pips.at(pip).dstWire;
    pips.at(pip).bound_net = net;
    wires.at(wire).bound_net = net;
    net->wires[wire].pip = pip;
    net->wires[wire].strength = strength;
    refreshUiPip(pip);
    refreshUiWire(wire);
}

void Arch::unbindPip(PipId pip)
{
    WireId wire = pips.at(pip).dstWire;
    wires.at(wire).bound_net->wires.erase(wire);
    pips.at(pip).bound_net = nullptr;
    wires.at(wire).bound_net = nullptr;
    refreshUiPip(pip);
    refreshUiWire(wire);
}

bool Arch::checkPipAvail(PipId pip) const { return pips.at(pip).bound_net == nullptr; }

NetInfo *Arch::getBoundPipNet(PipId pip) const { return pips.at(pip).bound_net; }

NetInfo *Arch::getConflictingPipNet(PipId pip) const { return pips.at(pip).bound_net; }

WireId Arch::getConflictingPipWire(PipId pip) const { return pips.at(pip).bound_net ? pips.at(pip).dstWire : WireId(); }

const std::vector<PipId> &Arch::getPips() const { return pip_ids; }

Loc Arch::getPipLocation(PipId pip) const { return pips.at(pip).loc; }

WireId Arch::getPipSrcWire(PipId pip) const { return pips.at(pip).srcWire; }

WireId Arch::getPipDstWire(PipId pip) const { return pips.at(pip).dstWire; }

DelayInfo Arch::getPipDelay(PipId pip) const { return pips.at(pip).delay; }

const std::vector<PipId> &Arch::getPipsDownhill(WireId wire) const { return wires.at(wire).downhill; }

const std::vector<PipId> &Arch::getPipsUphill(WireId wire) const { return wires.at(wire).uphill; }

const std::vector<PipId> &Arch::getWireAliases(WireId wire) const { return wires.at(wire).aliases; }

// ---------------------------------------------------------------

GroupId Arch::getGroupByName(IdString name) const { return name; }

IdString Arch::getGroupName(GroupId group) const { return group; }

std::vector<GroupId> Arch::getGroups() const
{
    std::vector<GroupId> ret;
    for (auto &it : groups)
        ret.push_back(it.first);
    return ret;
}

const std::vector<BelId> &Arch::getGroupBels(GroupId group) const { return groups.at(group).bels; }

const std::vector<WireId> &Arch::getGroupWires(GroupId group) const { return groups.at(group).wires; }

const std::vector<PipId> &Arch::getGroupPips(GroupId group) const { return groups.at(group).pips; }

const std::vector<GroupId> &Arch::getGroupGroups(GroupId group) const { return groups.at(group).groups; }

// ---------------------------------------------------------------

delay_t Arch::estimateDelay(WireId src, WireId dst) const
{
    const WireInfo &s = wires.at(src);
    const WireInfo &d = wires.at(dst);
    int dx = abs(s.x - d.x);
    int dy = abs(s.y - d.y);
    return (dx + dy) * args.delayScale + args.delayOffset;
}

delay_t Arch::predictDelay(const NetInfo *net_info, const PortRef &sink) const
{
    const auto &driver = net_info->driver;
    auto driver_loc = getBelLocation(driver.cell->bel);
    auto sink_loc = getBelLocation(sink.cell->bel);

    int dx = abs(sink_loc.x - driver_loc.x);
    int dy = abs(sink_loc.y - driver_loc.y);
    return (dx + dy) * args.delayScale + args.delayOffset;
}

bool Arch::getBudgetOverride(const NetInfo *net_info, const PortRef &sink, delay_t &budget) const { return false; }

ArcBounds Arch::getRouteBoundingBox(WireId src, WireId dst) const
{
    ArcBounds bb;

    int src_x = wires.at(src).x;
    int src_y = wires.at(src).y;
    int dst_x = wires.at(dst).x;
    int dst_y = wires.at(dst).y;

    bb.x0 = src_x;
    bb.y0 = src_y;
    bb.x1 = src_x;
    bb.y1 = src_y;

    auto extend = [&](int x, int y) {
        bb.x0 = std::min(bb.x0, x);
        bb.x1 = std::max(bb.x1, x);
        bb.y0 = std::min(bb.y0, y);
        bb.y1 = std::max(bb.y1, y);
    };
    extend(dst_x, dst_y);
    return bb;
}

// ---------------------------------------------------------------

bool Arch::place()
{
    std::string placer = str_or_default(settings, id("placer"), defaultPlacer);
    if (placer == "heap") {
//        bool have_iobuf_or_constr = false;
//        for (auto cell : sorted(cells)) {
//            CellInfo *ci = cell.second;
//            if (ci->type == id("BORCA_IOB") || ci->bel != BelId() || ci->attrs.count(id("BEL"))) {
//                have_iobuf_or_constr = true;
//                break;
//            }
//        }
//        bool retVal;
//        if (!have_iobuf_or_constr) {
//            log_warning("Unable to use HeAP due to a lack of IO buffers or constrained cells as anchors; reverting to "
//                        "SA.\n");
//            retVal = placer1(getCtx(), Placer1Cfg(getCtx()));
//        } else {
//            PlacerHeapCfg cfg(getCtx());
//            cfg.ioBufTypes.insert(id("BORCA_IOB"));
//            retVal = placer_heap(getCtx(), cfg);
//        }
        PlacerHeapCfg cfg(getCtx());
        bool retVal = placer_heap(getCtx(), cfg);
        getCtx()->settings[getCtx()->id("place")] = 1;
        archInfoToAttributes();
        return retVal;
    } else if (placer == "sa") {
        bool retVal = placer1(getCtx(), Placer1Cfg(getCtx()));
        getCtx()->settings[getCtx()->id("place")] = 1;
        archInfoToAttributes();
        return retVal;
    } else {
        log_error("Borca architecture does not support placer '%s'\n", placer.c_str());
    }
}

bool Arch::route()
{
    std::string router = str_or_default(settings, id("router"), defaultRouter);
    bool result;
    if (router == "router1") {
        result = router1(getCtx(), Router1Cfg(getCtx()));
    } else if (router == "router2") {
        router2(getCtx(), Router2Cfg(getCtx()));
        result = true;
    } else {
        log_error("iCE40 architecture does not support router '%s'\n", router.c_str());
    }
    getCtx()->settings[getCtx()->id("route")] = 1;
    archInfoToAttributes();
    return result;
}

// ---------------------------------------------------------------

const std::vector<GraphicElement> &Arch::getDecalGraphics(DecalId decal) const
{
    if (!decal_graphics.count(decal)) {
        std::cerr << "No decal named " << decal.str(this) << std::endl;
        log_error("No decal named %s!\n", decal.c_str(this));
    }
    return decal_graphics.at(decal);
}

DecalXY Arch::getBelDecal(BelId bel) const { return bels.at(bel).decalxy; }

DecalXY Arch::getWireDecal(WireId wire) const { return wires.at(wire).decalxy; }

DecalXY Arch::getPipDecal(PipId pip) const { return pips.at(pip).decalxy; }

DecalXY Arch::getGroupDecal(GroupId group) const { return groups.at(group).decalxy; }

// ---------------------------------------------------------------

bool Arch::getCellDelay(const CellInfo *cell, IdString fromPort, IdString toPort, DelayInfo &delay) const
{
    if (!cellTiming.count(cell->name))
        return false;
    const auto &tmg = cellTiming.at(cell->name);
    auto fnd = tmg.combDelays.find(CellDelayKey{fromPort, toPort});
    if (fnd != tmg.combDelays.end()) {
        delay = fnd->second;
        return true;
    } else {
        return false;
    }
}

// Get the port class, also setting clockPort if applicable
TimingPortClass Arch::getPortTimingClass(const CellInfo *cell, IdString port, int &clockInfoCount) const
{
    if (!cellTiming.count(cell->name))
        return TMG_IGNORE;
    const auto &tmg = cellTiming.at(cell->name);
    if (tmg.clockingInfo.count(port))
        clockInfoCount = int(tmg.clockingInfo.at(port).size());
    else
        clockInfoCount = 0;
    return get_or_default(tmg.portClasses, port, TMG_IGNORE);
}

TimingClockingInfo Arch::getPortClockingInfo(const CellInfo *cell, IdString port, int index) const
{
    NPNR_ASSERT(cellTiming.count(cell->name));
    const auto &tmg = cellTiming.at(cell->name);
    NPNR_ASSERT(tmg.clockingInfo.count(port));
    return tmg.clockingInfo.at(port).at(index);
}

bool Arch::isValidBelForCell(CellInfo *cell, BelId bel) const
{
    std::vector<const CellInfo *> cells;
    cells.push_back(cell);
    Loc loc = getBelLocation(bel);
    for (auto tbel : getBelsByTile(loc.x, loc.y)) {
        if (tbel == bel)
            continue;
        CellInfo *bound = getBoundBelCell(tbel);
        if (bound != nullptr)
            cells.push_back(bound);
    }
    return cellsCompatible(cells.data(), int(cells.size()));
}

bool Arch::isBelLocationValid(BelId bel) const
{
    std::vector<const CellInfo *> cells;
    Loc loc = getBelLocation(bel);
    for (auto tbel : getBelsByTile(loc.x, loc.y)) {
        CellInfo *bound = getBoundBelCell(tbel);
        if (bound != nullptr)
            cells.push_back(bound);
    }
    return cellsCompatible(cells.data(), int(cells.size()));
}

#ifdef WITH_HEAP
const std::string Arch::defaultPlacer = "heap";
#else
const std::string Arch::defaultPlacer = "sa";
#endif

const std::vector<std::string> Arch::availablePlacers = {"sa",
#ifdef WITH_HEAP
                                                         "heap"
#endif
};

const std::string Arch::defaultRouter = "router1";
const std::vector<std::string> Arch::availableRouters = {"router1", "router2"};

void Arch::assignArchInfo()
{
    for (auto &cell : getCtx()->cells) {
        CellInfo *ci = cell.second.get();
        if (ci->type == id("LUT4") || ci->type == id("DFFER") || ci->type == id("CARRY4")) {
            ci->is_slice = true;
            ci->slice_clk = get_net_or_empty(ci, id("CLK"));
        } else {
            ci->is_slice = false;
        }
        ci->user_group = int_or_default(ci->attrs, id("PACK_GROUP"), -1);
    }
}

bool Arch::cellsCompatible(const CellInfo **cells, int count) const
{
    const NetInfo *clk = nullptr;
    int group = -1;
    for (int i = 0; i < count; i++) {
        const CellInfo *ci = cells[i];
        if (ci->is_slice && ci->slice_clk != nullptr) {
            if (clk == nullptr)
                clk = ci->slice_clk;
            else if (clk != ci->slice_clk)
                return false;
        }
        if (ci->user_group != -1) {
            if (group == -1)
                group = ci->user_group;
            else if (group != ci->user_group)
                return false;
        }
    }
    return true;
}

NEXTPNR_NAMESPACE_END

/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2019  David Shah <david@symbioticeda.com>
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

#include "cells.h"
#include "design_utils.h"
#include "log.h"
#include "util.h"
#include <iostream>

NEXTPNR_NAMESPACE_BEGIN

void add_port(const Context *ctx, CellInfo *cell, std::string name, PortType dir)
{
    IdString id = ctx->id(name);
    NPNR_ASSERT(cell->ports.count(id) == 0);
    cell->ports[id] = PortInfo{id, nullptr, dir};
}

std::unique_ptr<CellInfo> create_clb(Context *ctx, IdString type, std::string name)
{
    static int auto_idx = 0;
    std::unique_ptr<CellInfo> new_cell = std::unique_ptr<CellInfo>(new CellInfo());
    if (name.empty()) {
        new_cell->name = ctx->id("$nextpnr_" + type.str(ctx) + "_" + std::to_string(auto_idx++));
    } else {
        new_cell->name = ctx->id(name);
    }
    new_cell->type = type;
    if (type == ctx->id("CLB")) {
        new_cell->params[ctx->id("K")] = ctx->args.K;
        new_cell->params[ctx->id("LUT_INIT")] = 0;
        new_cell->params[ctx->id("DFF_INIT")] = 0;
        new_cell->params[ctx->id("FF_USED")] = 0;

        for (int i = 0; i < ctx->args.K; i++)
            add_port(ctx, new_cell.get(), "LUT1_I" + std::to_string(i), PORT_IN);
        for (int i = 0; i < ctx->args.K; i++)
            add_port(ctx, new_cell.get(), "LUT0_I" + std::to_string(i), PORT_IN);
        for (int i = 0; i < ctx->args.K; i++)
            add_port(ctx, new_cell.get(), "LUT3_I" + std::to_string(i), PORT_IN);
        for (int i = 0; i < ctx->args.K; i++)
            add_port(ctx, new_cell.get(), "LUT2_I" + std::to_string(i), PORT_IN);
        for (int i = 0; i < ctx->args.K; i++)
            add_port(ctx, new_cell.get(), "LUT5_I" + std::to_string(i), PORT_IN);
        for (int i = 0; i < ctx->args.K; i++)
            add_port(ctx, new_cell.get(), "LUT4_I" + std::to_string(i), PORT_IN);
        for (int i = 0; i < ctx->args.K; i++)
            add_port(ctx, new_cell.get(), "LUT7_I" + std::to_string(i), PORT_IN);
        for (int i = 0; i < ctx->args.K; i++)
            add_port(ctx, new_cell.get(), "LUT6_I" + std::to_string(i), PORT_IN);

        add_port(ctx, new_cell.get(), "MUX_I0", PORT_IN);
        add_port(ctx, new_cell.get(), "MUX_I1", PORT_IN);

        add_port(ctx, new_cell.get(), "CLK", PORT_IN);
        add_port(ctx, new_cell.get(), "CE", PORT_IN);
        add_port(ctx, new_cell.get(), "RST", PORT_IN);

        add_port(ctx, new_cell.get(), "COMB1_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "COMB0_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "COMB3_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "COMB2_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "COMB5_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "COMB4_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "COMB7_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "COMB6_O", PORT_OUT);

        add_port(ctx, new_cell.get(), "SYNC1_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "SYNC0_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "SYNC3_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "SYNC2_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "SYNC5_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "SYNC4_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "SYNC7_O", PORT_OUT);
        add_port(ctx, new_cell.get(), "SYNC6_O", PORT_OUT);
    } else {
        log_error("unable to create CLB!");
    }
    return new_cell;
}

std::unique_ptr<CellInfo> create_dff_cell(Context *ctx, IdString type, std::string name)
{
    static int auto_idx = 0;
    std::unique_ptr<CellInfo> new_cell = std::unique_ptr<CellInfo>(new CellInfo());
    if (name.empty()) {
        new_cell->name = ctx->id("$nextpnr_" + type.str(ctx) + "_" + std::to_string(auto_idx++));
    } else {
        new_cell->name = ctx->id(name);
    }
    new_cell->type = type;
    if (type == ctx->id("DFFER")) {
        new_cell->params[ctx->id("INIT")] = 0;

        add_port(ctx, new_cell.get(), "CLK", PORT_IN);
        add_port(ctx, new_cell.get(), "CE", PORT_IN);
        add_port(ctx, new_cell.get(), "RST", PORT_IN);
        add_port(ctx, new_cell.get(), "D", PORT_IN);
        add_port(ctx, new_cell.get(), "Q", PORT_OUT);
    } else {
        log_error("unable to create DFF cell!");
    }
    return new_cell;
}

void lut_to_lc(const Context *ctx, CellInfo *lut, CellInfo *lc, bool no_dff)
{
    lc->params[ctx->id("LUT_INIT")] = lut->params[ctx->id("INIT")];

    int lut_k = int_or_default(lut->params, ctx->id("K"), 4);
    NPNR_ASSERT(lut_k <= ctx->args.K);

    for (int i = 0; i < lut_k; i++) {
        IdString lutPort = ctx->id("I" + std::to_string(i));
        IdString clbPort = ctx->id("LUT1_I" + std::to_string(i));
        replace_port(lut, lutPort, lc, clbPort);
    }

    if (no_dff) {
        lc->params[ctx->id("FF_USED")] = 0;
        replace_port(lut, ctx->id("O"), lc, ctx->id("COMB1_O"));
    }
}

void dff_to_lc(const Context *ctx, CellInfo *dff, CellInfo *lc, bool pass_thru_lut)
{
    lc->params[ctx->id("FF_USED")] = 1;
    lc->params[ctx->id("DFF_INIT")] = dff->params[ctx->id("INIT")];

    replace_port(dff, ctx->id("clk"), lc, ctx->id("CLK"));
    replace_port(dff, ctx->id("e"), lc, ctx->id("CE"));
    replace_port(dff, ctx->id("r"), lc, ctx->id("RST"));

    if (pass_thru_lut) {
        // Fill LUT with alternating 10
        const int init_size = 1 << lc->params[ctx->id("K")].as_int64();
        std::string init;
        init.reserve(init_size);
        for (int i = 0; i < init_size; i += 2)
            init.append("10");
        lc->params[ctx->id("LUT_INIT")] = Property::from_string(init);

        replace_port(dff, ctx->id("d"), lc, ctx->id("LUT1_I0"));
    }

    replace_port(dff, ctx->id("q"), lc, ctx->id("SYNC1_O"));
}

void vio_to_dff(const Context *ctx, CellInfo *io, CellInfo *dff)
{
    dff->params[ctx->id("INIT")] = 1;
    if (io->type == ctx->id("$nextpnr_ibuf"))
        replace_port(io, ctx->id("O"), dff, ctx->id("Q"));
    else if  (io->type == ctx->id("$nextpnr_obuf"))
        replace_port(io, ctx->id("I"), dff, ctx->id("D"));
}

//void nxio_to_iob(Context *ctx, CellInfo *nxio, CellInfo *iob, std::unordered_set<IdString> &todelete_cells)
//{
//    if (nxio->type == ctx->id("$nextpnr_ibuf")) {
//        iob->params[ctx->id("INPUT_USED")] = 1;
//        replace_port(nxio, ctx->id("O"), iob, ctx->id("O"));
//    } else if (nxio->type == ctx->id("$nextpnr_obuf")) {
//        iob->params[ctx->id("OUTPUT_USED")] = 1;
//        replace_port(nxio, ctx->id("I"), iob, ctx->id("I"));
//    } else if (nxio->type == ctx->id("$nextpnr_iobuf")) {
//        // N.B. tristate will be dealt with below
//        iob->params[ctx->id("INPUT_USED")] = 1;
//        iob->params[ctx->id("OUTPUT_USED")] = 1;
//        replace_port(nxio, ctx->id("I"), iob, ctx->id("I"));
//        replace_port(nxio, ctx->id("O"), iob, ctx->id("O"));
//    } else {
//        NPNR_ASSERT(false);
//    }
//    NetInfo *donet = iob->ports.at(ctx->id("I")).net;
//    CellInfo *tbuf = net_driven_by(
//            ctx, donet, [](const Context *ctx, const CellInfo *cell) { return cell->type == ctx->id("$_TBUF_"); },
//            ctx->id("Y"));
//    if (tbuf) {
//        iob->params[ctx->id("ENABLE_USED")] = 1;
//        replace_port(tbuf, ctx->id("A"), iob, ctx->id("I"));
//        replace_port(tbuf, ctx->id("E"), iob, ctx->id("EN"));
//
//        if (donet->users.size() > 1) {
//            for (auto user : donet->users)
//                log_info("     remaining tristate user: %s.%s\n", user.cell->name.c_str(ctx), user.port.c_str(ctx));
//            log_error("unsupported tristate IO pattern for IO buffer '%s', "
//                      "instantiate BORCA_IOB manually to ensure correct behaviour\n",
//                      nxio->name.c_str(ctx));
//        }
//        ctx->nets.erase(donet->name);
//        todelete_cells.insert(tbuf->name);
//    }
//}

NEXTPNR_NAMESPACE_END

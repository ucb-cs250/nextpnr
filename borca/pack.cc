/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2018-19  David Shah <david@symbioticeda.com>
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

#include <algorithm>
#include <iterator>
#include <unordered_set>
#include "cells.h"
#include "design_utils.h"
#include "log.h"
#include "util.h"

#include <iostream>

NEXTPNR_NAMESPACE_BEGIN

// Pack LUTs
static void pack_luts(Context *ctx)
{
    log_info("Packing LUTs..\n");

    std::unordered_set<IdString> packed_cells;
    std::vector<std::unique_ptr<CellInfo>> new_cells;
    for (auto cell : sorted(ctx->cells)) {
        CellInfo *ci = cell.second;
        if (ctx->verbose)
            log_info("cell '%s' is of type '%s'\n", ci->name.c_str(ctx), ci->type.c_str(ctx));
        if (is_lut(ctx, ci)) {
            std::unique_ptr<CellInfo> packed =
                    create_clb(ctx, ctx->id("CLB"), ci->name.str(ctx) + "_LUT4");
            std::copy(ci->attrs.begin(), ci->attrs.end(), std::inserter(packed->attrs, packed->attrs.begin()));
            packed_cells.insert(ci->name);
            if (ctx->verbose)
                log_info("packed cell %s into %s\n", ci->name.c_str(ctx), packed->name.c_str(ctx));
            lut_to_lc(ctx, ci, packed.get(), true);
            new_cells.push_back(std::move(packed));
        }
    }
    for (auto pcell : packed_cells) {
        ctx->cells.erase(pcell);
    }
    for (auto &ncell : new_cells) {
        ctx->cells[ncell->name] = std::move(ncell);
    }
}

// Pack FFs
static void pack_ffs(Context *ctx)
{
    log_info("Packing non-LUT FFs..\n");

    std::unordered_set<IdString> packed_cells;
    std::vector<std::unique_ptr<CellInfo>> new_cells;

    for (auto cell : sorted(ctx->cells)) {
        CellInfo *ci = cell.second;
        if (is_ff(ctx, ci)) {
            for (auto p : ci->ports) {
              std::cout << "DFF Ports " << p.first.str(ctx) << '\n';
            }
            std::unique_ptr<CellInfo> packed =
                    create_clb(ctx, ctx->id("CLB"), ci->name.str(ctx) + "_DFF");
            std::copy(ci->attrs.begin(), ci->attrs.end(), std::inserter(packed->attrs, packed->attrs.begin()));
            packed_cells.insert(ci->name);
            dff_to_lc(ctx, ci, packed.get(), true);
            new_cells.push_back(std::move(packed));
        }
    }
    for (auto pcell : packed_cells) {
        ctx->cells.erase(pcell);
    }
    for (auto &ncell : new_cells) {
        ctx->cells[ncell->name] = std::move(ncell);
    }
}

// Merge a net into a constant net
static void set_net_constant(const Context *ctx, NetInfo *orig, NetInfo *constnet, bool constval)
{
    orig->driver.cell = nullptr;
    for (auto user : orig->users) {
        if (user.cell != nullptr) {
            CellInfo *uc = user.cell;
            if (ctx->verbose)
                log_info("%s user %s\n", orig->name.c_str(ctx), uc->name.c_str(ctx));
            if ((is_lut(ctx, uc) || is_lc(ctx, uc)) && (user.port.str(ctx).at(0) == 'I') && !constval) {
                uc->ports[user.port].net = nullptr;
            } else {
                uc->ports[user.port].net = constnet;
                constnet->users.push_back(user);
            }
        }
    }
    orig->users.clear();
}

// Pack constants (simple implementation)
static void pack_constants(Context *ctx)
{
    log_info("Packing constants..\n");

    std::unique_ptr<CellInfo> gnd_cell = create_clb(ctx, ctx->id("BORCA_CELL"), "$PACKER_GND");
    gnd_cell->params[ctx->id("INIT")] = Property(0, 1 << ctx->args.K);
    std::unique_ptr<NetInfo> gnd_net = std::unique_ptr<NetInfo>(new NetInfo);
    gnd_net->name = ctx->id("$PACKER_GND_NET");
    gnd_net->driver.cell = gnd_cell.get();
    gnd_net->driver.port = ctx->id("F");
    gnd_cell->ports.at(ctx->id("F")).net = gnd_net.get();

    std::unique_ptr<CellInfo> vcc_cell = create_clb(ctx, ctx->id("BORCA_CELL"), "$PACKER_VCC");
    // Fill with 1s
    vcc_cell->params[ctx->id("INIT")] = Property(Property::S1).extract(0, (1 << ctx->args.K), Property::S1);
    std::unique_ptr<NetInfo> vcc_net = std::unique_ptr<NetInfo>(new NetInfo);
    vcc_net->name = ctx->id("$PACKER_VCC_NET");
    vcc_net->driver.cell = vcc_cell.get();
    vcc_net->driver.port = ctx->id("F");
    vcc_cell->ports.at(ctx->id("F")).net = vcc_net.get();

    std::vector<IdString> dead_nets;

    bool gnd_used = false;

    for (auto net : sorted(ctx->nets)) {
        NetInfo *ni = net.second;
        if (ni->driver.cell != nullptr && ni->driver.cell->type == ctx->id("GND")) {
            IdString drv_cell = ni->driver.cell->name;
            set_net_constant(ctx, ni, gnd_net.get(), false);
            gnd_used = true;
            dead_nets.push_back(net.first);
            ctx->cells.erase(drv_cell);
        } else if (ni->driver.cell != nullptr && ni->driver.cell->type == ctx->id("VCC")) {
            IdString drv_cell = ni->driver.cell->name;
            set_net_constant(ctx, ni, vcc_net.get(), true);
            dead_nets.push_back(net.first);
            ctx->cells.erase(drv_cell);
        }
    }

    if (gnd_used) {
        ctx->cells[gnd_cell->name] = std::move(gnd_cell);
        ctx->nets[gnd_net->name] = std::move(gnd_net);
    }
    // Vcc cell always inserted for now, as it may be needed during carry legalisation (TODO: trim later if actually
    // never used?)
    ctx->cells[vcc_cell->name] = std::move(vcc_cell);
    ctx->nets[vcc_net->name] = std::move(vcc_net);

    for (auto dn : dead_nets) {
        ctx->nets.erase(dn);
    }
}

static bool is_nextpnr_iob(Context *ctx, CellInfo *cell)
{
    return cell->type == ctx->id("$nextpnr_ibuf") || cell->type == ctx->id("$nextpnr_obuf") ||
           cell->type == ctx->id("$nextpnr_iobuf");
}

static void pack_virtual_io(Context *ctx)
{
    std::unordered_set<IdString> packed_cells;
    std::vector<std::unique_ptr<CellInfo>> new_cells;
    std::unordered_set<IdString> clk_iocells;
    log_info("Packing IOs..\n");

    // Find clock-related IOB
    for (auto &n : ctx->nets) {
        auto ni = n.second.get();
        for (auto user : ni->users) {
            // Find a net that drives a DFF, then check if that net connects
            // to port "CLK" of the DFF
            if (!is_ff(ctx, user.cell))
                continue;
            if (ni == user.cell->ports.at(ctx->id("CLK")).net) {
                if (is_nextpnr_iob(ctx, ni->driver.cell))
                  clk_iocells.insert(ni->driver.cell->name);
            }
        }
    }

    for (auto cell : sorted(ctx->cells)) {
        CellInfo *ci = cell.second;
        // If this is a clock iocell, we don't convert it to a DFF
        if (clk_iocells.find(ci->name) != clk_iocells.end())
            continue;
        if (is_nextpnr_iob(ctx, ci)) {
            std::unique_ptr<CellInfo> packed =
                    create_dff_cell(ctx, ctx->id("DFFER"), ci->name.str(ctx) + "_DFF");
            if (ctx->verbose)
                log_info("packed cell %s into %s\n", ci->name.c_str(ctx), packed->name.c_str(ctx));
            packed_cells.insert(ci->name);
            vio_to_dff(ctx, ci, packed.get());
            new_cells.push_back(std::move(packed));
        }
    }

    for (auto pcell : packed_cells) {
        ctx->cells.erase(pcell);
    }
    for (auto &ncell : new_cells) {
        ctx->cells[ncell->name] = std::move(ncell);
    }
}

//// Pack IO buffers
//static void pack_io(Context *ctx)
//{
//    std::unordered_set<IdString> packed_cells;
//    std::unordered_set<IdString> delete_nets;
//
//    std::vector<std::unique_ptr<CellInfo>> new_cells;
//    log_info("Packing IOs..\n");
//
//    for (auto cell : sorted(ctx->cells)) {
//        CellInfo *ci = cell.second;
//        if (is_nextpnr_iob(ctx, ci)) {
//            CellInfo *iob = nullptr;
//            if (ci->type == ctx->id("$nextpnr_ibuf") || ci->type == ctx->id("$nextpnr_iobuf")) {
//                iob = net_only_drives(ctx, ci->ports.at(ctx->id("O")).net, is_borca_iob, ctx->id("PAD"), true, ci);
//
//            } else if (ci->type == ctx->id("$nextpnr_obuf")) {
//                NetInfo *net = ci->ports.at(ctx->id("I")).net;
//                iob = net_only_drives(ctx, net, is_borca_iob, ctx->id("PAD"), true, ci);
//            }
//            if (iob != nullptr) {
//                // Trivial case, BORCA_IOB used. Just destroy the net and the
//                // iobuf
//                log_info("%s feeds BORCA_IOB %s, removing %s %s.\n", ci->name.c_str(ctx), iob->name.c_str(ctx),
//                         ci->type.c_str(ctx), ci->name.c_str(ctx));
//                NetInfo *net = iob->ports.at(ctx->id("PAD")).net;
//                if (((ci->type == ctx->id("$nextpnr_ibuf") || ci->type == ctx->id("$nextpnr_iobuf")) &&
//                     net->users.size() > 1) ||
//                    (ci->type == ctx->id("$nextpnr_obuf") && (net->users.size() > 2 || net->driver.cell != nullptr)))
//                    log_error("PAD of %s '%s' connected to more than a single top level IO.\n", iob->type.c_str(ctx),
//                              iob->name.c_str(ctx));
//
//                if (net != nullptr) {
//                    delete_nets.insert(net->name);
//                    iob->ports.at(ctx->id("PAD")).net = nullptr;
//                }
//                if (ci->type == ctx->id("$nextpnr_iobuf")) {
//                    NetInfo *net2 = ci->ports.at(ctx->id("I")).net;
//                    if (net2 != nullptr) {
//                        delete_nets.insert(net2->name);
//                    }
//                }
//            } else if (bool_or_default(ctx->settings, ctx->id("disable_iobs"))) {
//                // No IO buffer insertion; just remove nextpnr_[io]buf
//                for (auto &p : ci->ports)
//                    disconnect_port(ctx, ci, p.first);
//            } else {
//                // Create a BORCA_IOB buffer
//                std::unique_ptr<CellInfo> ice_cell =
//                        create_clb(ctx, ctx->id("BORCA_IOB"), ci->name.str(ctx) + "$iob");
//                nxio_to_iob(ctx, ci, ice_cell.get(), packed_cells);
//                new_cells.push_back(std::move(ice_cell));
//                iob = new_cells.back().get();
//            }
//            packed_cells.insert(ci->name);
//            if (iob != nullptr)
//                std::copy(ci->attrs.begin(), ci->attrs.end(), std::inserter(iob->attrs, iob->attrs.begin()));
//        }
//    }
//    for (auto pcell : packed_cells) {
//        ctx->cells.erase(pcell);
//    }
//    for (auto dnet : delete_nets) {
//        ctx->nets.erase(dnet);
//    }
//    for (auto &ncell : new_cells) {
//        ctx->cells[ncell->name] = std::move(ncell);
//    }
//}

// Main pack function
bool Arch::pack()
{
    Context *ctx = getCtx();
    try {
        log_break();
        //pack_constants(ctx);
        //pack_io(ctx);
        //pack_luts(ctx);
        //pack_ffs(ctx);
        pack_virtual_io(ctx);
        ctx->settings[ctx->id("pack")] = 1;
        ctx->assignArchInfo();
        log_info("Checksum: 0x%08x\n", ctx->checksum());
        return true;
    } catch (log_execution_error_exception) {
        return false;
    }
}

NEXTPNR_NAMESPACE_END

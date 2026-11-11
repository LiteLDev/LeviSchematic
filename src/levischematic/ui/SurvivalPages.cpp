#include "SurvivalPages.h"
#include "levischematic/app/AppKernel.h"
#include <charconv>
#include <sstream>

namespace levischematic::ui {
namespace {
struct MaterialDraft {
    std::string search;
    bool        missingOnly = false, layerOnly = false;
    int         sort   = 0;
    size_t      offset = 0;
};
void materials(std::shared_ptr<Page> const& p, std::shared_ptr<MaterialDraft> const& draft) {
    p->rows.clear();
    p->title     = "材料清单 / Materials";
    auto session = app::accessSession();
    if (!session) {
        p->message = "No world session / 世界未就绪";
        return;
    }
    auto        report  = session->survival().materials().report();
    auto        rebuild = [p, draft] { materials(p, draft); };
    PageBuilder b{p};
    b.button("refresh", "刷新 / Refresh", rebuild);
    b.button("scan", "重新统计 / Recalculate", [p, draft] {
        auto s = app::accessSession();
        if (s) s->survival().materials().request(draft->layerOnly);
        materials(p, draft);
    });
    b.input("search", "搜索 / Search", draft->search, [draft, rebuild](std::string text) {
        draft->search = std::move(text);
        draft->offset = 0;
        rebuild();
    });
    b.input(
        "missing",
        "只看尚需材料 / Remaining only",
        draft->missingOnly ? "true" : "false",
        [draft, rebuild](std::string) {
            draft->missingOnly = !draft->missingOnly;
            draft->offset      = 0;
            rebuild();
        }
    );
    p->rows.back().kind = RowKind::Toggle;
    b.input(
        "layer",
        "仅当前渲染层 / Current layers",
        draft->layerOnly ? "true" : "false",
        [draft, rebuild](std::string) {
            draft->layerOnly = !draft->layerOnly;
            draft->offset    = 0;
            auto s           = app::accessSession();
            if (s) s->survival().materials().request(draft->layerOnly);
            rebuild();
        }
    );
    p->rows.back().kind = RowKind::Toggle;
    b.input("sort", "排序 / Sort", std::to_string(draft->sort), [draft, rebuild](std::string text) {
        int n             = 0;
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), n);
        if (error == std::errc{} && end == text.data() + text.size() && n >= 0 && n < 3) {
            draft->sort   = n;
            draft->offset = 0;
            rebuild();
        }
    });
    p->rows.back().kind    = RowKind::Choice;
    p->rows.back().choices = {"名称 / Name", "总量 / Total", "需准备 / To gather"};
    b.button("export", "导出 CSV / Export CSV", [p] {
        auto s = app::accessSession();
        if (s) p->message = s->survival().materials().exportCsv(s->runtime().schematicDirectory());
    });
    b.button("options", "辅助设置 / Survival options", [] { requestOpen(PageId::Survival); });
    b.info(
        "summary",
        "Placement #" + std::to_string(report.placementId) + " | "
            + (report.building ? "Scanning / 统计中 "
               : report.stale  ? "Stale / 待刷新 "
                               : "Ready / 已完成 ")
            + std::to_string(report.scanned) + " / " + std::to_string(report.total)
            + " | Unresolved / 未解析: " + std::to_string(report.unresolved)
    );
    if (report.secondLayer)
        b.info(
            "layer_warning",
            "第二层方块尚未纳入统计，请核对含水结构 / Secondary layer not included; check waterlogged structures"
        );
    size_t warning = 0;
    for (auto const& text : report.warnings)
        if (warning < 4) b.info("warning" + std::to_string(warning++), text);
    std::erase_if(report.rows, [&](auto const& row) {
        return (draft->missingOnly && !row.counts.remaining())
            || (!draft->search.empty() && row.label.find(draft->search) == std::string::npos
                && row.item.name.find(draft->search) == std::string::npos);
    });
    std::stable_sort(report.rows.begin(), report.rows.end(), [&](auto const& a, auto const& b) {
        if (draft->sort == 1 && a.counts.total != b.counts.total) return a.counts.total > b.counts.total;
        if (draft->sort == 2 && a.counts.toGather() != b.counts.toGather())
            return a.counts.toGather() > b.counts.toGather();
        return a.item < b.item;
    });
    if (draft->offset >= report.rows.size()) draft->offset = 0;
    for (size_t i = draft->offset; i < report.rows.size() && i < draft->offset + 24; ++i) {
        auto const& row = report.rows[i];
        auto const& c   = row.counts;
        auto detail     = row.item.name + " aux=" + std::to_string(row.item.aux) + " | 总量 " + std::to_string(c.total)
                        + " 缺失 " + std::to_string(c.missing) + " 类型错 " + std::to_string(c.wrongBlock) + " 状态错 "
                        + std::to_string(c.wrongState) + " 未知 " + std::to_string(c.unknown) + " 背包 "
                        + std::to_string(c.available) + " 需准备 " + std::to_string(c.toGather());
        b.button(
            "material" + std::to_string(i),
            row.label + " | " + std::to_string(c.toGather()) + " / " + std::to_string(c.available) + " / "
                + std::to_string(c.total),
            [p, detail] { p->message = detail; }
        );
        p->rows.back().kind = RowKind::TableRow;
        p->rows.back().columns = {row.label, std::to_string(c.toGather()), std::to_string(c.available), std::to_string(c.total)};
    }
    b.button("previous", "上一页 / Previous", [draft, rebuild] {
        draft->offset = draft->offset >= 24 ? draft->offset - 24 : 0;
        rebuild();
    });
    b.button("next", "下一页 / Next", [draft, rebuild] {
        draft->offset += 24;
        rebuild();
    });
    b.info(
        "columns",
        "行：需准备 / 背包 / 总量；点击查看明细。未知数量不计为完成。 / Click for details; unknown remains unresolved."
    );
}
void options(std::shared_ptr<Page> const& p) {
    p->rows.clear();
    p->title = "生存辅助 / Survival assistance";
    PageBuilder b{p};
    auto        cfg = app::configStore().snapshot();
    using S         = app::Config::Survival;
    for (auto const& [id, label, member] : {
             std::tuple{"pick",         "投影取物 / Pick projection",       &S::pickBlock           },
             {"easy",         "Easy Place / 辅助放置",            &S::easyPlace           },
             {"restriction",  "放置限制 / Placement restriction", &S::placementRestriction},
             {"hold",         "持续使用 / Hold to place",         &S::holdToPlace         },
             {"material_hud", "材料 HUD / Material HUD",          &S::materialHud         },
             {"verifier_hud", "校验位置 HUD / Verifier HUD",      &S::verifierHud         }
    }) {
        b.input(id, label, cfg.survival.*member ? "true" : "false", [p, member, id = std::string(id)](std::string) {
            auto c             = app::configStore().snapshot();
            c.survival.*member = !(c.survival.*member);
            auto error         = app::configStore().submit(c);
            if (error.empty()) error = app::configStore().save();
            p->message = error.empty() ? "已保存，下 tick 生效 / Saved" : error;
            if (error.empty())
                for (auto& row : p->rows)
                    if (row.id == id) row.value = c.survival.*member ? "true" : "false";
        });
        p->rows.back().kind = RowKind::Toggle;
    }
    b.input("delay", "换物等待毫秒 / Swap delay ms", std::to_string(cfg.survival.swapDelayMs), [p](std::string text) {
        int value;
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size()) {
            p->message = "Integer required / 请输入整数";
            return;
        }
        auto c                 = app::configStore().snapshot();
        c.survival.swapDelayMs = value;
        p->message             = app::configStore().submit(c);
        if (p->message.empty()) p->message = app::configStore().save();
    });
    std::string slots;
    for (int slot : cfg.survival.hotbarSlots) {
        if (!slots.empty()) slots += ",";
        slots += std::to_string(slot + 1);
    }
    b.input("slots", "允许替换的槽位 1–9 / Pick slots", slots, [p](std::string text) {
        std::vector<int>   slots;
        std::istringstream stream(text);
        std::string        part;
        while (std::getline(stream, part, ',')) {
            int n             = 0;
            auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), n);
            if (error != std::errc{} || end != part.data() + part.size()) {
                p->message = "Use comma-separated slots / 用逗号分隔槽位";
                return;
            }
            slots.push_back(n - 1);
        }
        auto c                 = app::configStore().snapshot();
        c.survival.hotbarSlots = slots;
        p->message             = app::configStore().submit(c);
        if (p->message.empty()) p->message = app::configStore().save();
    });
    b.button("materials", "材料清单 / Materials", [] { requestOpen(PageId::Materials); });
    b.info(
        "help",
        "中键取物，M+中键开关取物，按住 I 查看方块；Easy Place 启用后使用键执行，复杂状态不支持时跳过。 / Hold I for "
        "block information."
    );
}
} 
void populateSurvivalPage(std::shared_ptr<Page> const& p, PageId id) {
    if (id == PageId::Materials) {
        auto draft = std::make_shared<MaterialDraft>();
        auto s     = app::accessSession();
        if (s) s->survival().materials().request(false);
        materials(p, draft);
    } else options(p);
}
} 

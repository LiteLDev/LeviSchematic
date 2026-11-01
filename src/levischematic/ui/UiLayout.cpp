#include "UiLayout.h"

namespace levischematic::ui {
nlohmann::ordered_json buildNativeDefinitions() {
    using J   = nlohmann::ordered_json;
    auto bind = [](std::string const& name, std::string target = {}) {
        return J::array({
            J{{"binding_type", "global"},
              {"binding_name", name},
              {"binding_condition", "always"},
              {"binding_name_override", target.empty() ? name : target}}
        });
    };
    auto button = [](std::string name, std::string text, std::string action) {
        return J{
            {name + "@common_buttons.light_text_button",
             J{{"layer", 2},
               {"$pressed_button_name", action},
               {"$button_text", text},
               {"$button_text_binding_type", "global"},
               {"$button_binding_condition", "always"},
               {"$button_text_max_size", {"100% - 6px", 16}},
               {"focus_enabled", true}}}
        };
    };
    auto label = [&](std::string text) {
        return J{
            {"type",     "label"           },
            {"text",     text              },
            {"localize", false             },
            {"shadow",   false             },
            {"color",    {0.25, 0.25, 0.25}},
            {"layer",    2                 },
            {"bindings", bind(text)        }
        };
    };
    J definitions = J::object();
    
    
    definitions["tab_caption"] = J{
        {"type",           "label"                },
        {"size",           {"100%", "100%"}       },
        {"text",           "$ls_tab_caption"      },
        {"localize",       false                  },
        {"shadow",         false                  },
        {"color",          "$text_color"          },
        {"text_alignment", "center"               },
        {"bindings",       bind("$ls_tab_caption")}
    };
    J   tabControls  = J::array();
    J   bodyControls = J::array({J{{"background@common.dialog_background_opaque", J{{"layer", 0}}}}});
    int tab          = 0;
    for (auto const* suffix : {"projection", "settings", "keys"}) {
        std::string n          = suffix;
        std::string collection = "ls_rows_" + n;
        auto        cell       = button("action", "#ls_row_text", "button.ls_row_" + n);
        auto&       action     = cell.begin().value();
        action["size"]         = {"100% - 4px", 20};
        action["anchor_from"] = action["anchor_to"] = "top_middle";
        action["$button_text_binding_type"]         = "collection";
        action["$button_text_grid_collection_name"] = collection;
        action["bindings"]                          = J::array({
            J{{"binding_type", "collection"},
              {"binding_collection_name", collection},
              {"binding_name", "#ls_row_id"},
              {"binding_condition", "always"}}
        });
        definitions["row_" + n]                     = J{
            {"type",     "panel"     },
            {"size",     {"100%", 22}},
            {"controls",
             J::array(
                 {cell,
                  J{{"selection@common.focus_border_white",
                     J{{"size", {"100% - 4px", 20}},
                       {"anchor_from", "top_middle"},
                       {"anchor_to", "top_middle"},
                       {"layer", 5},
                       {"visible", false},
                       {"bindings",
                        J::array(
                            {J{{"binding_type", "collection"},
                               {"binding_collection_name", collection},
                               {"binding_name", "#ls_row_selected"},
                               {"binding_name_override", "#visible"},
                               {"binding_condition", "always"}}}
                        )}}}}}
             )                       }
        };
        definitions["content_" + n] = J{
            {"type",                   "grid"                  },
            {"size",                   {"100%", "default"}     },
            {"anchor_from",            "top_left"              },
            {"anchor_to",              "top_left"              },
            {"grid_item_template",     "levischematic.row_" + n},
            {"grid_dimension_binding", "#ls_dims_" + n         },
            {"collection_name",        collection              },
            {"bindings",               bind("#ls_dims_" + n)   }
        };
        tabControls.push_back(
            J{
                {"tab_" + n + "@common_tabs.tab_top",
                 J{{"size", {"33.333333%", "100%"}},
                   {"layer", 3},
                   {"$tab_view_binding_name", "ls_tab_" + n},
                   {"$toggle_name", "ls_tabs"},
                   {"$radio_toggle_group", true},
                   {"$toggle_group_forced_index", tab},
                   {"$toggle_group_default_selected", 0},
                   {"$toggle_default_state", tab == 0},
                   {"$toggle_binding_type", "global"},
                   {"$toggle_binding_condition", "always"},
                   {"$toggle_state_binding_name", "#ls_visible_" + n},
                   {"$toggle_reset_on_focus_lost", false},
                   {"$ignore_tts_enabled_bind", true},
                   {"$toggle_focus_bind", "#ls_tabs_focus"},
                   {"$toggle_focus_enabled", true},
                   {"$tab_content", "levischematic.tab_caption"},
                   {"$ls_tab_caption", "#ls_tab_" + n},
                   {"$is_left_most_tab", tab == 0},
                   {"$is_right_most_tab", tab == 2}}}
        }
        );
        
        
        bodyControls.push_back(
            J{
                {"scroll_" + n + "@common.container_scroll_panel",
                 J{{"size", {"100% - 20px", "100% - 74px"}},
                   {"offset", {0, 42}},
                   {"anchor_from", "top_middle"},
                   {"anchor_to", "top_middle"},
                   {"layer", 2},
                   {"visible", tab == 0},
                   {"bindings", bind("#ls_visible_" + n, "#visible")},
                   {"$scrolling_content", "levischematic.content_" + n},
                   {"$scroll_view_name", "ls_scroll_view_" + n},
                   {"$scrolling_panel_base_button_mappings", J::array()},
                   {"$scroll_panel_consume_hover_events", false},
                   {"$show_background", true},
                   {"$scroll_view_port_clips_children", true},
                   {"$scroll_size", {6, "100%"}},
                   {"$scroll_bar_left_padding_size", {2, 0}},
                   {"$scroll_bar_right_padding_size", {0, 0}}}}
        }
        );
        ++tab;
    }
    auto title = label("#ls_title");
    title.update(
        J{
            {"size",        {"100% - 80px", 12}},
            {"offset",      {10, 8}            },
            {"anchor_from", "top_left"         },
            {"anchor_to",   "top_left"         }
    }
    );
    bodyControls.push_back(
        J{
            {"title", title}
    }
    );
    auto close = button("back", "#ls_back", "button.ls_back");
    close.begin().value().update(
        J{
            {"size",        {52, 18}   },
            {"offset",      {-8, 6}    },
            {"anchor_from", "top_right"},
            {"anchor_to",   "top_right"}
    }
    );
    bodyControls.push_back(close);
    auto message = label("#ls_message");
    message.update(
        J{
            {"size",        {"100% - 20px", 14}},
            {"offset",      {0, 25}            },
            {"anchor_from", "top_middle"       },
            {"anchor_to",   "top_middle"       }
    }
    );
    bodyControls.push_back(
        J{
            {"message", message}
    }
    );
    bodyControls.push_back(
        J{
            {"editor@common.text_edit_box",
             J{{"size", {"70% - 12px", 20}},
               {"offset", {10, -8}},
               {"anchor_from", "bottom_left"},
               {"anchor_to", "bottom_left"},
               {"layer", 2},
               {"visible", false},
               {"bindings", bind("#ls_editing", "#visible")},
               {"max_length", 1024},
               {"enabled", true},
               {"$text_box_name", "ls_editor"},
               {"$text_edit_box_enabled_binding_type", "none"},
               {"$text_edit_box_content_binding_name", "#ls_edit"},
               {"$text_edit_box_content_binding_type", "global"},
               {"$text_edit_box_binding_condition", "always"}}}
    }
    );
    auto commit = button("commit", "#ls_commit", "button.ls_commit");
    commit.begin().value().update(
        J{
            {"size", {"30% - 12px", 20}},
            {"offset", {-10, -8}},
            {"anchor_from", "bottom_right"},
            {"anchor_to", "bottom_right"},
            {"visible", false},
            {"bindings", bind("#ls_editing", "#visible")}
    }
    );
    bodyControls.push_back(commit);
    auto hint = label("#ls_hint");
    hint.update(
        J{
            {"size",        {"100% - 20px", 20}},
            {"offset",      {0, -8}            },
            {"anchor_from", "bottom_middle"    },
            {"anchor_to",   "bottom_middle"    }
    }
    );
    hint["bindings"].push_back(bind("#ls_not_editing", "#visible").at(0));
    bodyControls.push_back(
        J{
            {"hint", hint}
    }
    );
    J mappings = J::array();
    for (auto from : {"button.menu_cancel", "button.menu_exit", "button.menu_inventory_cancel"})
        mappings.push_back(
            J{
                {"from_button_id", from            },
                {"to_button_id",   "button.ls_back"},
                {"mapping_type",   "global"        }
        }
        );
    definitions["screen"] = J{
        {"type",                   "screen"        },
        {"size",                   {"100%", "100%"}},
        {"always_listen_to_input", true            },
        {"is_modal",               true            },
        {"absorbs_input",          true            },
        {"render_game_behind",     true            },
        {"should_steal_mouse",     false           },
        {"button_mappings",        mappings        },
        {"controls",
         J::array(
             {J{{"screen_background@common.screen_background", J{{"size", {"100%", "100%"}}}}},
              J{{"panel",
                 J{{"type", "panel"},
                   {"size", {"94%", "100% - 20px"}},
                   {"max_size", {420, 248}},
                   {"controls",
                    J::array(
                        {J{{"tab_strip",
                            J{{"type", "stack_panel"},
                              {"orientation", "horizontal"},
                              {"size", {"100%", 28}},
                              {"anchor_from", "top_left"},
                              {"anchor_to", "top_left"},
                              {"layer", 3},
                              {"controls", tabControls}}}},
                         J{{"body",
                            J{{"type", "panel"},
                              {"size", {"100%", "100% - 26px"}},
                              {"offset", {0, 26}},
                              {"anchor_from", "top_left"},
                              {"anchor_to", "top_left"},
                              {"layer", 1},
                              {"controls", bodyControls}}}}}
                    )}}}}}
         )                                         }
    };
    return definitions;
}
nlohmann::ordered_json buildNativeDefinition() { return buildNativeDefinitions().at("screen"); }
} 

/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// our own tools of the editor (ficzery2), kept out of editormode.cpp so the merges of the others' work touch it less:
// the hourglass of the heavy work, the camera settings, the WMS presets, the orthophoto fetched first where the camera
// looks, the list of the surfaces cut out of the terrain and the filled areas
#include "stdafx.h"
#include "world/TrackSleepers.h"
#include <filesystem>
#include <iterator>
#include <fstream>
#include <regex>
#include "application/editormode.h"
#include "application/editormodeshared.h"
#include "application/editorgestures.h"
#include "scene/scenenodegroups.h"
#include "application/editoruilayer.h"
#ifndef _WIN32
#include <spawn.h>
#include <unistd.h>
extern char **environ;
#endif
#include "application/editorprojection.h"

#include "application/application.h"
#include "editor/editorSettings.hpp"
#include "editor/editorModelSets.hpp"
#include "editor/editorTrackPresets.hpp"
#include "editor/editorIncludeInfo.hpp"
#include "editor/editorGroundMesh.hpp"
#include "editor/editorFormat.hpp"
#include "editor/editorAlignmentStore.hpp"
#include "editor/editorSwitchStore.hpp"
#include "world/catenary_scene.h"
#include "editor/editorGeometry.hpp"
#include "editor/editorIcons.hpp"
#include "utilities/translation.h"
#include "utilities/Globals.h"
#include "utilities/glmHelpers.h"
#include "simulation/simulation.h"
#include "simulation/simulationtime.h"
#include "simulation/simulationenvironment.h"
#include "widgets/weatherpanel.h"
#include "utilities/Timer.h"
#include "Console.h"
#include "rendering/renderer.h"
#include "model/AnimModel.h"
#include "model/Model3d.h"
#include "world/Track.h"
#include "utilities/Float3d.h"
#include "scene/scene.h"
#include "scene/scenelayers.h"
#include "scene/sceneterrain.h"
#include "utilities/parser.h"
#include "utilities/utilities.h"


#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/ImGuizmo.h"
#include "utilities/Logs.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>


namespace
{
std::string g_busy_task; // of the work going on, empty when there's none
std::chrono::steady_clock::time_point g_busy_shown;
bool g_busy_cancelled{false};
} // namespace

bool editor_busy_cancelled()
{
    return g_busy_cancelled;
}

bool editor_busy_progress(double const Fraction)
{
    if (g_busy_task.empty())
        return true;
    auto const now{std::chrono::steady_clock::now()};
    if (now - g_busy_shown < std::chrono::milliseconds(150))
        return false == g_busy_cancelled;
    g_busy_shown = now;
#ifdef _WIN32
    // a look at the queue of the window keeps Windows from taking it for hung (Not responding), without handling anything
    MSG message;
    ::PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    // Esc, while the simulator is the window in front
    DWORD process{0};
    ::GetWindowThreadProcessId(::GetForegroundWindow(), &process);
    if (process == ::GetCurrentProcessId() && (::GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
        g_busy_cancelled = true;
#endif
    // below 0: no news of how far, only the window kept answering
    if (Fraction >= 0.0)
    {
        auto const percent{static_cast<int>(std::clamp(Fraction, 0.0, 1.0) * 100.0)};
        Application.set_title(format("%s  %s %d%%  (%s)", Global.AppName.c_str(), g_busy_task.c_str(), percent, STR_C("Esc stops it")));
    }
    return false == g_busy_cancelled;
}

void editor_mode::run_busy(std::string Task, std::function<void()> Work)
{
    if (m_busy.pending)
        return; // one at a time
    m_busy.pending = true;
    m_busy.shown = 0;
    m_busy.task = std::move(Task);
    m_busy.work = std::move(Work);
}

void editor_mode::busy_run_pending()
{
    if (false == m_busy.pending || m_busy.shown < 1)
        return;
    auto work{std::move(m_busy.work)};
    m_busy.pending = false;
    g_busy_task = m_busy.task;
    g_busy_cancelled = false;
    g_busy_shown = std::chrono::steady_clock::now();
    Application.set_title(format("%s  %s  (%s)", Global.AppName.c_str(), g_busy_task.c_str(), STR_C("Esc stops it")));
    auto const start{std::chrono::steady_clock::now()};
    if (work)
        work();
    auto const seconds{std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()};
    WriteLog(format("Editor: %s took %.1f s%s", g_busy_task.c_str(), seconds, g_busy_cancelled ? " (stopped with Esc)" : ""));
    if (g_busy_cancelled)
        ui()->set_status(format(STR_C("%s stopped with Esc after %.0f s: what was done stays, Ctrl+Z takes it back"), g_busy_task.c_str(), seconds), false);
    g_busy_task.clear();
    g_busy_cancelled = false;
    Application.set_title(Global.AppName);
}

void editor_mode::draw_busy_overlay()
{
    if (false == m_busy.pending)
        return;
    ++m_busy.shown;
    auto *drawlist{ImGui::GetForegroundDrawList(ImGui::GetMainViewport())};
    auto const &io{ImGui::GetIO()};
    drawlist->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(0, 0, 0, 140));
    ImVec2 const centre{io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f};
    // the hourglass: two triangles in a frame, the sand in the lower one
    ImU32 const yellow{IM_COL32(255, 210, 60, 255)};
    float const h{46.0f}, w{30.0f};
    ImVec2 const top{centre.x, centre.y - 40.0f};
    drawlist->AddLine(ImVec2(top.x - w, top.y - h), ImVec2(top.x + w, top.y - h), yellow, 4.0f);
    drawlist->AddLine(ImVec2(top.x - w, top.y + h), ImVec2(top.x + w, top.y + h), yellow, 4.0f);
    drawlist->AddTriangle(ImVec2(top.x - w + 5.0f, top.y - h + 3.0f), ImVec2(top.x + w - 5.0f, top.y - h + 3.0f), top, yellow, 3.0f);
    drawlist->AddTriangle(ImVec2(top.x - w + 5.0f, top.y + h - 3.0f), ImVec2(top.x + w - 5.0f, top.y + h - 3.0f), top, yellow, 3.0f);
    drawlist->AddTriangleFilled(ImVec2(top.x - w + 12.0f, top.y + h - 6.0f), ImVec2(top.x + w - 12.0f, top.y + h - 6.0f), ImVec2(top.x, top.y + 16.0f), yellow);
    auto const title{m_busy.task + "..."};
    auto const note{std::string{STR_C("Working, the screen stays as it is until it's done. Esc stops it where the work allows; the title of the window shows how far it got")}};
    auto const size{ImGui::CalcTextSize(title.c_str())};
    auto const notesize{ImGui::CalcTextSize(note.c_str())};
    auto const y{top.y + h + 22.0f};
    auto const width{std::max(size.x * 1.4f, notesize.x) + 40.0f};
    drawlist->AddRectFilled(ImVec2(centre.x - width * 0.5f, y - 10.0f), ImVec2(centre.x + width * 0.5f, y + size.y * 1.4f + notesize.y + 24.0f), IM_COL32(20, 22, 26, 235), 6.0f);
    drawlist->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.4f, ImVec2(centre.x - size.x * 0.7f, y), yellow, title.c_str());
    drawlist->AddText(ImVec2(centre.x - notesize.x * 0.5f, y + size.y * 1.4f + 10.0f), IM_COL32(230, 230, 230, 230), note.c_str());
}

bool editor_mode::cursor_on_ground(glm::dvec3 &World)
{
    if (m_streamer.active() && terrain_cursor(World))
        return true;
    glm::dvec3 const offset = GfxRenderer->Mouse_Position();
    auto const depth{glm::length(offset) > 1e-3};
    // no terrain under the ray: the ground models or the terrain model, along the ray from the level of the depth read back
    auto const start{depth ? Camera.Pos.y + offset.y : Camera.Pos.y - 2.0};
    auto const point{cursor_terrain(start)};
    if (std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) && glm::distance(point, glm::dvec3{Camera.Pos}) < 20000.0)
    {
        World = point;
        return true;
    }
    if (false == depth)
        return false;
    World = Camera.Pos + offset;
    return true;
}

void editor_mode::orthophoto_hurry()
{
    if (false == m_orthophoto.enabled())
        m_orthophoto.enabled(true);
    // what the camera sees of the ground: rays through a grid over the screen to the terrain, or as far as the reach of the
    // orthophoto along the ground where they don't meet it (the sky, the edge of the loaded terrain)
    ImGuiIO const &io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f)
        return;
    auto const inverse = glm::inverse(editor_mode::projection_matrix(io.DisplaySize.x / io.DisplaySize.y) * GfxRenderer->Camera_View_Matrix());
    auto const camera{GfxRenderer->Camera_Position()};
    double constexpr reach{2500.0};
    glm::dvec2 low{camera.x, camera.z}, high{camera.x, camera.z};
    for (int j = 0; j <= 6; ++j)
        for (int i = 0; i <= 6; ++i)
        {
            glm::vec2 const ndc{i / 3.0f - 1.0f, j / 3.0f - 1.0f};
            auto const point = [&](float const Depth) {
                auto const clip = inverse * glm::vec4(ndc, Depth, 1.0f);
                return glm::dvec3{glm::vec3{clip} / clip.w};
            };
            auto direction = point(0.5f) - point(0.0f);
            if (glm::length(direction) < 1e-9)
                continue;
            direction = glm::normalize(direction);
            glm::dvec3 hit;
            if (false == m_streamer.raycast(camera + point(0.0f), direction, reach, hit))
            {
                glm::dvec2 flat{direction.x, direction.z};
                if (glm::length(flat) < 1e-6)
                    continue;
                flat = glm::normalize(flat) * reach;
                hit = {camera.x + flat.x, 0.0, camera.z + flat.y};
            }
            low = glm::min(low, glm::dvec2{hit.x, hit.z});
            high = glm::max(high, glm::dvec2{hit.x, hit.z});
        }
    auto const count{m_orthophoto.hurry(low, high)};
    ui()->set_status(count > 0 ? format(STR_C("Orthophoto: %zu tiles of the view downloaded first (Ctrl+Tab)"), count)
                           : std::string{STR("Orthophoto: the view is all shown already (Ctrl+Tab)")});
}

void editor_mode::render_patches_ui()
{
    auto const &patches = m_streamer.patches();
    if (patches.empty())
        return;
    ImGui::Separator();
    ImGui::Text(STR_C("Surfaces cut out of the terrain: %zu"), patches.size());
    // a filter by name, for the many cut along the tracks (e.g. "nasyp", "L132 torowisko")
    static char filter[96]{};
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##patchfilter", STR_C("Filter by name, words apart by spaces..."), filter, sizeof(filter));
    std::vector<std::string> words;
    {
        std::istringstream split{std::string{filter}};
        std::string word;
        while (split >> word)
        {
            std::transform(word.begin(), word.end(), word.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
            words.push_back(word);
        }
    }
    // filtered again only when the filter or the list changes: tens of thousands of names a frame cost a frame
    static std::vector<int> shown;
    static std::string shown_filter;
    static void const *shown_revision{nullptr};
    static std::size_t shown_count{0};
    if (shown_filter != filter || shown_revision != static_cast<void const *>(patches.data()) || shown_count != patches.size())
    {
        shown_filter = filter;
        shown_revision = static_cast<void const *>(patches.data());
        shown_count = patches.size();
        shown.clear();
        shown.reserve(patches.size());
        for (int i = 0; i < static_cast<int>(patches.size()); ++i)
        {
            auto name{patches[i].name};
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
            if (std::all_of(words.begin(), words.end(), [&](std::string const &Word) { return name.find(Word) != std::string::npos; }))
                shown.push_back(i);
        }
    }
    if (false == words.empty())
    {
        ImGui::TextDisabled(STR_C("%zu shown"), shown.size());
        auto const &layers = m_streamer.manifest().layers;
        ImGui::SameLine();
        ImGui::BeginDisabled(layers.empty() || shown.empty());
        if (ImGui::SmallButton(STR_C("Material of the palette to all shown")))
        {
            auto const &chosen = layers[std::clamp(m_terrain_layer, 0, static_cast<int>(layers.size()) - 1)];
            auto all{patches};
            std::vector<std::size_t> changed;
            for (auto const index : shown)
            {
                all[index].material = chosen.material;
                all[index].size = chosen.size;
                all[index].rotation = chosen.rotation;
                changed.push_back(static_cast<std::size_t>(index));
            }
            repaint_patches(std::move(all), changed);
            m_terrain_status = format(STR_C("%s given to %zu surfaces, Ctrl+Z takes it back"), chosen.material.c_str(), shown.size());
            ImGui::EndDisabled();
            return;
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", STR_C("The material chosen in the palette, with its repeat and rotation, to all the surfaces the filter shows"));
    }
    std::optional<std::pair<std::size_t, std::string>> dropped;
    if (ImGui::BeginListBox("##patches", ImVec2(-FLT_MIN, std::min(8.0f, static_cast<float>(shown.size()) + 0.5f) * ImGui::GetTextLineHeightWithSpacing())))
    {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(shown.size()));
        while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
        {
            auto const i{static_cast<std::size_t>(shown[row])};
            ImGui::PushID(static_cast<int>(i));
            auto const material = patches[i].material.empty() ? std::string(STR_C("default material")) : patches[i].material;
            if (ImGui::Selectable(format(STR_C("%s, %zu points, %s"), patches[i].name.c_str(), patches[i].outline.size(), material.c_str()).c_str(), static_cast<int>(i) == m_patch_selected))
                m_patch_selected = static_cast<int>(i);
            // a texture dropped on an entry becomes the material of that surface
            std::string name;
            if (material_drop(name))
                dropped = std::make_pair(i, name);
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    if (dropped)
    {
        auto changed{patches};
        changed[dropped->first].material = dropped->second;
        repaint_patches(std::move(changed), {dropped->first});
        m_patch_selected = static_cast<int>(dropped->first);
        return;
    }
    if (m_patch_selected < 0 || m_patch_selected >= static_cast<int>(patches.size()))
        return;
    auto patch{patches[m_patch_selected]};
    bool changed{false};
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s", patch.material.c_str());
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputText(STR_C("Material##patch"), buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
        patch.material = buffer, changed = true;
    std::string name;
    if (material_drop(name))
        patch.material = name, changed = true;
    ImGui::SameLine();
    auto const &layers = m_streamer.manifest().layers;
    ImGui::BeginDisabled(layers.empty());
    if (ImGui::Button(STR_C("From the palette##patch")))
    {
        auto const &chosen = layers[std::clamp(m_terrain_layer, 0, static_cast<int>(layers.size()) - 1)];
        patch.material = chosen.material;
        patch.size = chosen.size;
        patch.rotation = chosen.rotation;
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat(STR_C("Repeat (m)##patch"), &patch.size, 0.1f, 0.0f, 500.0f, patch.size > 0.0f ? "%.1f" : STR_C("of the material"));
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat(STR_C("Turned by (deg)##patch"), &patch.rotation, -180.0f, 180.0f, "%.0f");
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    if (patch.paint)
    {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat(STR_C("Transition (m)##patch"), &patch.feather, 0.05f, 0.0f, 20.0f, patch.feather > 0.0f ? "%.2f" : STR_C("sharp"));
        changed |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat(STR_C("Noise of the edge (m)##patch"), &patch.noise, 0.05f, 0.0f, 10.0f, patch.noise > 0.0f ? "%.2f" : STR_C("none"));
        changed |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat(STR_C("Waves of the noise (m)##patch"), &patch.noise_scale, 0.1f, 0.5f, 50.0f, "%.1f");
        changed |= ImGui::IsItemDeactivatedAfterEdit();
        changed |= grass_choice(STR_C("3D grass under it##patch"), patch.grass);
    }
    else
    {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat(STR_C("Lift (m)##patch"), &patch.lift, 0.01f, -5.0f, 5.0f, "%.2f");
        changed |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", STR_C("Metres the surface lies over the heights taken from the terrain"));
    }
    if (changed)
    {
        patch.size = std::max(0.0f, patch.size);
        patch.feather = std::max(0.0f, patch.feather);
        patch.noise = std::max(0.0f, patch.noise);
        patch.noise_scale = std::max(0.5f, patch.noise_scale);
        auto all{patches};
        all[m_patch_selected] = std::move(patch);
        repaint_patches(std::move(all), {static_cast<std::size_t>(m_patch_selected)});
        return;
    }
    if (ImGui::Button(STR_C("Outline it again##patch")))
    {
        if (false == patch.shape.empty() && restore_outline(patch.shape))
            return;
        m_paint_outline.clear();
        for (auto const &point : patch.outline)
        {
            double height{0.0};
            patch.height(point.x, point.y, height);
            m_paint_outline.emplace_back(point.x, height, point.y);
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", STR_C("Puts the outline of the surface back as the outline of the Fill tool"));
    ImGui::SameLine();
    if (ImGui::Button(STR_C("Go to##patch")))
    {
        glm::dvec2 low;
        glm::dvec2 high;
        patch.bounds(low, high);
        go_to_plan((low + high) * 0.5);
    }
    ImGui::SameLine();
    if (ImGui::Button(STR_C("Give back to the terrain##patch")))
    {
        auto const gone = patch;
        auto all{patches};
        all.erase(all.begin() + m_patch_selected);
        m_patch_selected = -1;
        if (gone.paint)
        {
            // the first material of the palette over it again, the ones around get their edges back
            repaint_patches(std::move(all), {}, {gone});
            m_terrain_status = STR_C("Surface removed, the ground under it has the first material of the palette again");
            return;
        }
        m_streamer.cut(gone.outline, false);
        m_streamer.patches(std::move(all));
        forget_ground();
        m_terrain_status = STR_C("Surface removed, the hole under it closed");
    }
}

namespace
{
// the public WMS services of Poland drawing in PUWG 1992, ready to tick. Some layers are named as the service has them; where the names
// aren't known for sure, the layers are found in the capabilities by words of their names and titles, all of them where none match
struct wms_preset
{
    char const *title;
    char const *what;
    char const *url;
    char const *layers; // the names, comma separated; none: by the words below
    std::vector<char const *> words;
};

std::vector<wms_preset> const &wms_presets()
{
    static std::vector<wms_preset> const presets{
        {"Railway lines and stations (PKP PLK)", "The lines of PKP PLK with their numbers, the stations and the stops, the plants of the lines",
         "http://mapa.plk-sa.pl:8080/geoserver/SILK/ows", "WMS_SILK_linie_kolejowe,WMS_SILK_stacje_przystanki,WMS_SILK_zaklady", {}},
        {"Railways (BDOT10k)", "The tracks of the topographic database, platforms and the railway areas", "https://mapy.geoportal.gov.pl/wss/service/pub/guest/kompozycja_BDOT10k_WMS/MapServer/WMSServer", "",
         {"kolej", "torow", "tory", "sktr", "peron"}},
        {"Power lines and poles (BDOT10k)", "Overhead power lines, their poles, masts and towers", "https://mapy.geoportal.gov.pl/wss/service/pub/guest/kompozycja_BDOT10k_WMS/MapServer/WMSServer", "",
         {"energ", "przesy", "suln", "slup", "słup", "maszt", "wież", "wiez"}},
        {"Utility networks (KIUT)", "Power, telecommunication, water, sewage, gas and heating lines of the districts; seen zoomed in",
         "https://integracja.gugik.gov.pl/cgi-bin/KrajowaIntegracjaUzbrojeniaTerenu", "", {}},
        {"Buildings (land register)", "The outlines of the buildings of the land register of the districts", "https://integracja.gugik.gov.pl/cgi-bin/KrajowaIntegracjaEwidencjiGruntow", "budynki", {}},
        {"Buildings (BDOT10k)", "The buildings of the topographic database", "https://mapy.geoportal.gov.pl/wss/service/pub/guest/kompozycja_BDOT10k_WMS/MapServer/WMSServer", "", {"budyn", "bubd"}},
        {"Parcel boundaries", "The parcels of the land register with their numbers; seen zoomed in", "https://integracja.gugik.gov.pl/cgi-bin/KrajowaIntegracjaEwidencjiGruntow", "dzialki,numery_dzialek", {}},
        {"Land use (land register)", "The kinds of the land of the register: arable, meadows, forest, built up...", "https://integracja.gugik.gov.pl/cgi-bin/KrajowaIntegracjaEwidencjiGruntow", "",
         {"uzytk", "użytk"}},
        {"Forests and woods (BDOT10k)", "Forests, woods and bushes of the topographic database", "https://mapy.geoportal.gov.pl/wss/service/pub/guest/kompozycja_BDOT10k_WMS/MapServer/WMSServer", "",
         {"lasy", "leśn", "lesn", "zadrzew", "ptlz", "krzew"}},
        {"Forests (Forest Data Bank)", "The forest compartments of the Forest Data Bank", "https://mapserver.bdl.lasy.gov.pl/ArcGIS/services/WMS_BDL/MapServer/WMSServer", "", {"wydziel", "oddzia", "las"}},
        {"Roads (BDOT10k)", "Roads, streets and their carriageways of the topographic database", "https://mapy.geoportal.gov.pl/wss/service/pub/guest/kompozycja_BDOT10k_WMS/MapServer/WMSServer", "",
         {"drog", "jezdn", "ulic", "skdr", "skjz"}},
        {"Waters (BDOT10k)", "Rivers, ditches and lakes of the topographic database", "https://mapy.geoportal.gov.pl/wss/service/pub/guest/kompozycja_BDOT10k_WMS/MapServer/WMSServer", "",
         {"wod", "rzek", "rów", "jezior", "ptwp", "swrs"}},
        {"Everything of BDOT10k", "The whole topographic database at once", "https://mapy.geoportal.gov.pl/wss/service/pub/guest/kompozycja_BDOT10k_WMS/MapServer/WMSServer", "", {}},
        {"Streets and addresses", "The streets, the squares and the address points of the national register", "https://integracja.gugik.gov.pl/cgi-bin/KrajowaIntegracjaNumeracjiAdresowej",
         "prg-ulice,prg-place,prg-adresy", {}},
        {"Local plans (MPZP)", "The local zoning plans of the communes", "https://mapy.geoportal.gov.pl/wss/ext/KrajowaIntegracjaMiejscowychPlanowZagospodarowaniaPrzestrzennego", "", {}},
        {"Listed monuments", "The immovable monuments of the register", "http://usluga.zabytek.gov.pl/INSPIRE_IMD/service.svc/get", "Immovable_Monuments", {}},
    };
    return presets;
}

// the layers of the preset the service has: those named, or those whose names or titles hold its words, or all of them
std::string preset_layers(wms_preset const &Preset, editor_orthophoto::service_info const &Info)
{
    std::string layers;
    auto const add = [&](std::string const &Name) {
        if (false == Name.empty())
            layers += (layers.empty() ? "" : ",") + Name;
    };
    if (*Preset.layers != '\0')
    {
        std::string const wanted{Preset.layers};
        std::size_t from{0};
        while (from <= wanted.size())
        {
            auto const comma{wanted.find(',', from)};
            auto const name{wanted.substr(from, comma == std::string::npos ? std::string::npos : comma - from)};
            // as named, where the capabilities weren't read or have no such layer the service is asked for it anyway
            auto const found{std::find_if(Info.layers.begin(), Info.layers.end(), [&](auto const &Layer) { return ToLower(Layer.first) == ToLower(name); })};
            add(found != Info.layers.end() ? found->first : name);
            if (comma == std::string::npos)
                break;
            from = comma + 1;
        }
        return layers;
    }
    for (auto const &[name, title] : Info.layers)
    {
        auto const text{ToLower(name + " " + title)};
        if (Preset.words.empty() || std::any_of(Preset.words.begin(), Preset.words.end(), [&](char const *Word) { return text.find(ToLower(Word)) != std::string::npos; }))
            add(name);
    }
    if (layers.empty() && false == Preset.words.empty())
        for (auto const &layer : Info.layers)
            add(layer.first);
    return layers;
}
} // namespace

void editor_mode::render_wms_presets()
{
    auto &stored{EditorSettings.wms_layers()};
    ImGui::PushID("wmspresets");
    if (false == ImGui::TreeNode("##presets", "%s", STR_C("Ready WMS layers: railways, poles, buildings, parcels...")))
    {
        ImGui::PopID();
        return;
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", STR_C("Public services of GUGiK, PKP PLK and others, drawn over the orthophoto as it lies. Ticked, a layer is added above and shown; its terms are in its entry"));
    ImGui::PopTextWrapPos();
    auto const &presets{wms_presets()};
    auto save{false};
    for (std::size_t i = 0; i < presets.size(); ++i)
    {
        auto const &preset{presets[i]};
        auto const title{STR(preset.title)};
        auto const entry{std::find_if(stored.begin(), stored.end(), [&](editorSettings::wms_layer const &Layer) { return Layer.url == preset.url && (Layer.title == title || Layer.title == preset.title); })};
        auto const reading{m_wms_preset_reading.count(i) != 0};
        bool ticked{entry != stored.end() && entry->visible};
        ImGui::PushID(static_cast<int>(i));
        if (reading)
            ImGui::BeginDisabled();
        if (ImGui::Checkbox(title.c_str(), &ticked))
        {
            if (entry != stored.end())
            {
                entry->visible = ticked;
                save = true;
            }
            else if (ticked)
            {
                // the layers of the service read first, on a thread of its own
                m_wms_preset_note.erase(i);
                std::string const url{preset.url};
                m_wms_preset_reading[i] = std::async(std::launch::async, [url]() { return editor_orthophoto::describe(url); });
            }
        }
        if (reading)
            ImGui::EndDisabled();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%s", STR_C(preset.what), preset.url);
        if (reading)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", STR_C("reading the service..."));
        }
        if (auto const note{m_wms_preset_note.find(i)}; note != m_wms_preset_note.end())
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", note->second.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::PopID();
    }
    // the services read: the layer added on top of the others, shown
    for (auto it = m_wms_preset_reading.begin(); it != m_wms_preset_reading.end();)
    {
        if (it->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            ++it;
            continue;
        }
        auto const info{it->second.get()};
        auto const &preset{presets[it->first]};
        auto const layers{preset_layers(preset, info)};
        if (false == info.layers.empty() && false == info.puwg)
            m_wms_preset_note[it->first] = STR("The service doesn't draw in PUWG 1992 (EPSG:2180), the editor can't lay it out");
        else if (layers.empty())
            m_wms_preset_note[it->first] = info.error.empty() ? std::string{STR_C("The service has no layers to show")} : format(STR_C("The service can't be read: %s"), info.error.c_str());
        else
        {
            if (false == info.error.empty())
                m_wms_preset_note[it->first] = format(STR_C("The service can't be read: %s"), info.error.c_str()) + "\n" + STR("The layers are asked for as named, they may not come");
            editorSettings::wms_layer entry;
            entry.url = preset.url;
            entry.layers = layers;
            entry.title = STR(preset.title);
            entry.terms = info.terms;
            entry.opacity = 0.9f;
            stored.insert(stored.begin(), entry);
            save = true;
        }
        it = m_wms_preset_reading.erase(it);
    }
    if (save)
        EditorSettings.save();
    ImGui::TreePop();
    ImGui::PopID();
}

void editor_mode::apply_camera_settings()
{
    auto const &camera{EditorSettings.camera()};
    Global.EditorCameraSpeed = camera.speed;
    Global.EditorCameraFast = camera.fast;
    Global.EditorCameraZoomed = camera.zoomed;
    Global.EditorNoFog = camera.no_fog;
}

void editor_mode::render_camera_settings()
{
    auto &camera{EditorSettings.camera()};
    auto changed{false};
    auto release{false};
    ImGui::PushItemWidth(180.0f * Global.ui_scale);
    float speed{static_cast<float>(camera.speed)};
    if (ImGui::SliderFloat(STR_C("Speed of the camera"), &speed, 0.05f, 10.0f, "x %.2f", ImGuiSliderFlags_Logarithmic))
    {
        camera.speed = speed;
        changed = true;
    }
    release |= ImGui::IsItemDeactivatedAfterEdit();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", STR_C("How fast the arrows or WSAD move the camera, Shift a bit faster; 1 as it was"));
    float fast{static_cast<float>(camera.fast)};
    if (ImGui::SliderFloat(STR_C("Ctrl speeds it up"), &fast, 1.0f, 30.0f, "x %.1f", ImGuiSliderFlags_Logarithmic))
    {
        camera.fast = fast;
        changed = true;
    }
    release |= ImGui::IsItemDeactivatedAfterEdit();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", STR_C("How many times faster the camera goes with Ctrl held, Ctrl+Shift too; 30 as it was"));
    ImGui::PopItemWidth();
    if (ImGui::Checkbox(STR_C("In the top view as fast as the view is zoomed"), &camera.zoomed))
        changed = release = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", STR_C("Zoomed in the map moves slowly, zoomed out fast: across the screen at the same pace whatever the zoom"));
    ImGui::Separator();
    if (ImGui::Checkbox(STR_C("No fog in the editor"), &camera.no_fog))
        changed = release = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", STR_C("The fog and the haze of the air left out in the perspective view too, the textures of the ground seen in their own colours. In the top view they're always left out"));
    ImGui::Separator();
    if (ImGui::Checkbox(STR_C("Hold the wheel and drag: pan in the top view"), &camera.pan_ortho))
        changed = release = true;
    if (ImGui::Checkbox(STR_C("Hold the wheel and drag: pan in the perspective view too"), &camera.pan_perspective))
        changed = release = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", STR_C("The ground under the cursor goes along with it, the height and the turn of the camera stay"));
    ImGui::Separator();
    if (ImGui::MenuItem(STR_C("As it was")))
    {
        camera = {};
        changed = release = true;
    }
    if (changed)
        apply_camera_settings();
    if (release)
        EditorSettings.save();
}

// in the outline only: one material of the palette for another, the rest of the scenery as it was (the Fill tool of the terrain)
void editor_mode::render_replace_in_outline()
{
    auto const &layers{m_streamer.manifest().layers};
    if (false == layers.empty())
    {
        static int from{0};
        auto const order{m_streamer.layer_order()};
        from = std::clamp(from, 0, static_cast<int>(layers.size()) - 1);
        ImGui::SetNextItemWidth(170.0f);
        if (ImGui::BeginCombo(STR_C("Replace in the outline##replace"), layers[from].material.c_str()))
        {
            for (auto const i : order)
                if (ImGui::Selectable(layers[i].material.c_str(), static_cast<int>(i) == from))
                    from = i;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(m_paint_outline.size() < 3 || from == m_terrain_layer);
        if (change_button(STR_C("with the chosen one##replace")))
        {
            run_busy(STR_C("Replacing the material within the outline"), [this, from = static_cast<std::uint16_t>(from)]() {
                terrain_history_step();
                auto const done{m_streamer.replace_in_area(shaped_outline(), from, static_cast<std::uint16_t>(m_terrain_layer))};
                terrain_history_step();
                m_terrain_status = format(STR_C("Replaced in %zu chunks%s"), done.first,
                                          done.second > 0 ? format(STR_C("; %zu had no room for another material"), done.second).c_str() : "");
                if (done.first > 0)
                    m_paint_outline.clear();
            });
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", STR_C("Within the outline, what's painted with the material picked here gets the one chosen in the palette;\n"
                                          "elsewhere it stays. The magic wand makes the outline of a field in a click. Ctrl+Z takes it back"));
    }
}

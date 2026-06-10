#include "../Editor.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <cmath>

namespace sfs = std::filesystem;

namespace Indium
{
    // Visual authoring for cutscenes / timelines. Reads / writes the same
    // <project>/cutscenes/<name>.json format CutsceneManager runs at runtime (via the
    // shared ToJson / FromJson), so a timeline authored here plays back with no glue.
    //
    // The working copy is the Editor's csDoc_ (a Cutscene). Keys/events are kept in the
    // panel in whatever order they're edited — diamonds/markers are drawn at their times
    // regardless, and saveFile sorts a copy so the on-disk file (and the runtime, which
    // sorts on load) stay tidy. Selection is by stored index, so it survives edits that
    // don't remove items.
    namespace
    {
        sfs::path CutsceneDir(const std::string& projectPath)
        {
            return sfs::path(projectPath) / "cutscenes";
        }

        std::vector<std::string> ListCutscenes(const std::string& projectPath)
        {
            std::vector<std::string> out;
            std::error_code ec;
            const sfs::path dir = CutsceneDir(projectPath);
            if (!sfs::exists(dir, ec)) return out;
            for (const auto& e : sfs::directory_iterator(dir, ec))
            {
                if (ec) break;
                if (e.is_regular_file() && e.path().extension() == ".json")
                    out.push_back(e.path().stem().string());
            }
            std::sort(out.begin(), out.end());
            return out;
        }

        // ImGui InputText bound to a std::string via a heap buffer. Returns true on edit.
        bool StrField(const char* id, std::string& s, std::size_t cap = 96)
        {
            std::vector<char> buf(cap, '\0');
            std::strncpy(buf.data(), s.c_str(), cap - 1);
            const bool changed = ImGui::InputText(id, buf.data(), cap);
            if (changed) s.assign(buf.data());
            return changed;
        }

        // Trigger tracks fire by id/payload and need no entity (Dialogue/StoryFlag/Event),
        // or act on a bound entity (Audio/Animation/Activation/Particle). Camera binds the
        // primary camera when its target is blank.
        bool needsEntityTarget(CutsceneTrackType t)
        {
            return t == CutsceneTrackType::Transform || t == CutsceneTrackType::Audio
                || t == CutsceneTrackType::Animation || t == CutsceneTrackType::Activation
                || t == CutsceneTrackType::Particle;
        }

        // What a trigger event's `a` payload means — shown in the inspector / add hints.
        const char* payloadHint(CutsceneTrackType t)
        {
            switch (t)
            {
                case CutsceneTrackType::Dialogue:   return "dialogue id";
                case CutsceneTrackType::Audio:      return "\"play\" or \"stop\"";
                case CutsceneTrackType::Animation:  return "clip name";
                case CutsceneTrackType::Activation: return "\"show\" or \"hide\"";
                case CutsceneTrackType::StoryFlag:  return "flag name (b=\"false\" clears)";
                case CutsceneTrackType::Event:      return "NarrativeEvent tag";
                case CutsceneTrackType::Particle:   return "\"play\" or \"stop\"";
                default: return "";
            }
        }

        ImU32 trackColor(CutsceneTrackType t)
        {
            switch (t)
            {
                case CutsceneTrackType::Transform:  return IM_COL32(120, 200, 255, 255);
                case CutsceneTrackType::Camera:     return IM_COL32(255, 210, 90,  255);
                case CutsceneTrackType::Dialogue:   return IM_COL32(150, 230, 150, 255);
                case CutsceneTrackType::Audio:      return IM_COL32(230, 150, 230, 255);
                case CutsceneTrackType::Animation:  return IM_COL32(150, 200, 230, 255);
                case CutsceneTrackType::Activation: return IM_COL32(230, 180, 120, 255);
                case CutsceneTrackType::StoryFlag:  return IM_COL32(230, 200, 120, 255);
                case CutsceneTrackType::Event:      return IM_COL32(200, 200, 220, 255);
                case CutsceneTrackType::Particle:   return IM_COL32(200, 150, 255, 255);
            }
            return IM_COL32(200, 200, 200, 255);
        }

        const CutsceneTrackType kAllTypes[] = {
            CutsceneTrackType::Transform, CutsceneTrackType::Camera, CutsceneTrackType::Dialogue,
            CutsceneTrackType::Audio, CutsceneTrackType::Animation, CutsceneTrackType::Activation,
            CutsceneTrackType::StoryFlag, CutsceneTrackType::Event, CutsceneTrackType::Particle,
        };
    }

    void Editor::ShowCutscenes()
    {
        if (!pm.IsProjectOpen()) { ImGui::TextDisabled("No project open."); return; }

        const std::string projectPath = pm.GetCurrentProjectPath();
        CutsceneManager::Get().SetProjectPath(projectPath); // keep runtime in sync for preview

        // --- working-model helpers ------------------------------------------------

        auto pushUndo = [&]()
        {
            csUndo_.push_back(CutsceneManager::ToJson(csDoc_));
            while (csUndo_.size() > 64) csUndo_.pop_front();
        };
        auto doUndo = [&]()
        {
            if (csUndo_.empty()) return;
            csDoc_ = CutsceneManager::FromJson(csUndo_.back());
            csUndo_.pop_back();
            csSelTrack_ = csSelItem_ = -1;
            csDirty_ = true;
        };
        // Checkbox edits must push undo with the PRE-toggle document: ImGui::Checkbox
        // has already flipped a directly-bound bool by the time it returns true, so a
        // plain pushUndo() afterwards would snapshot the new state and make Undo a
        // no-op. Edit a copy, push, then commit.
        auto undoCheckbox = [&](const char* label, bool& v)
        {
            bool edited = v;
            if (!ImGui::Checkbox(label, &edited)) return;
            pushUndo();
            v = edited;
            csDirty_ = true;
        };

        // The entity a track drives (Camera with a blank target → primary camera).
        auto resolveBound = [&](const CutsceneTrack& tr) -> Entity*
        {
            if (tr.type == CutsceneTrackType::Camera && tr.target.empty())
            {
                for (const auto& e : scene.entities) if (e->getComponent<CameraComponent>()) return e.get();
                return nullptr;
            }
            for (const auto& e : scene.entities) if (e->name == tr.target) return e.get();
            return nullptr;
        };
        // Seed a new key from the bound entity's current transform, so "key here" captures
        // what's on screen — the move-in-viewport / key-on-timeline author loop.
        auto captureInto = [&](const CutsceneTrack& tr, CutsceneKey& k)
        {
            if (Entity* b = resolveBound(tr))
            {
                k.pos = b->position; k.rot = b->rotation; k.scale = b->scale;
                if (auto* cam = b->getComponent<CameraComponent>()) k.zoom = cam->zoom;
            }
        };
        auto addItemAt = [&](CutsceneTrack& tr, int trackIdx, float t)
        {
            pushUndo();
            if (tr.isInterpolated())
            {
                CutsceneKey k; k.time = t; captureInto(tr, k);
                tr.keys.push_back(k);
                csSelTrack_ = trackIdx; csSelItem_ = (int)tr.keys.size() - 1;
            }
            else
            {
                CutsceneEvent e; e.time = t;
                tr.events.push_back(e);
                csSelTrack_ = trackIdx; csSelItem_ = (int)tr.events.size() - 1;
            }
            csDirty_ = true;
        };

        auto loadFile = [&](const std::string& stem)
        {
            csExitPreview();   // restore any scrub preview before swapping documents
            csDoc_ = Cutscene{};
            csFile_.clear();
            csLoaded_ = false;
            csDirty_  = false;
            csSelTrack_ = csSelItem_ = -1;
            csUndo_.clear();

            const sfs::path path = CutsceneDir(projectPath) / (stem + ".json");
            std::ifstream f(path);
            if (!f.is_open()) { TraceLog(LOG_WARNING, "CUTSCENE: cannot open %s", path.string().c_str()); return; }
            nlohmann::json j;
            try { f >> j; }
            catch (...) { TraceLog(LOG_WARNING, "CUTSCENE: invalid JSON in %s", path.string().c_str()); return; }

            csDoc_      = CutsceneManager::FromJson(j);
            csDoc_.name = stem;
            csFile_     = stem;
            csLoaded_   = true;
        };

        auto saveFile = [&](const std::string& stem) -> bool
        {
            std::error_code ec;
            sfs::create_directories(CutsceneDir(projectPath), ec);

            // Sort a copy so the on-disk file is tidy without disturbing the working
            // copy's selection indices.
            Cutscene out = csDoc_;
            for (auto& t : out.tracks) CutsceneManager::SortTrack(t);
            const nlohmann::json j = CutsceneManager::ToJson(out);

            const sfs::path path = CutsceneDir(projectPath) / (stem + ".json");
            sfs::path tmp = path; tmp += ".tmp";
            {
                std::ofstream o(tmp);
                if (!o.is_open()) { TraceLog(LOG_ERROR, "CUTSCENE: cannot write %s", tmp.string().c_str()); return false; }
                o << std::setw(2) << j << std::endl;
            }
            sfs::rename(tmp, path, ec);
            if (ec) { TraceLog(LOG_WARNING, "CUTSCENE: save failed for %s (%s)", path.string().c_str(), ec.message().c_str()); return false; }
            csFile_  = stem;
            csDirty_ = false;
            TraceLog(LOG_INFO, "CUTSCENE: saved %s", path.string().c_str());
            return true;
        };

        auto doNew = [&](const std::string& stem)
        {
            csExitPreview();
            csDoc_          = Cutscene{};
            csDoc_.name     = stem;
            csDoc_.duration = 5.0f;
            csFile_         = stem;
            csLoaded_       = true;
            csDirty_        = true;
            csSelTrack_ = csSelItem_ = -1;
            csUndo_.clear();
        };

        auto requestLoad = [&](const std::string& stem)
        {
            if (stem.empty()) return;
            if (csLoaded_ && csDirty_) { csPendingAction_ = 1; csPendingArg_ = stem; }
            else                       { loadFile(stem); }
        };
        auto requestNew = [&](const std::string& stem)
        {
            if (stem.empty()) return;
            if (csLoaded_ && csDirty_) { csPendingAction_ = 2; csPendingArg_ = stem; }
            else                       { doNew(stem); }
        };

        // --- header + unsaved-changes guard ---------------------------------------
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::TextDisabled("   " ICON_FA_FILM "  Cutscene Timeline (project's cutscenes/ folder)");
        ImGui::PopStyleVar();
        ImGui::Separator();

        if (csPendingAction_ != 0)
        {
            const int         action = csPendingAction_;
            const std::string arg    = csPendingArg_;
            auto resolve = [&](bool saveFirst)
            {
                if (saveFirst && !csFile_.empty()) saveFile(csFile_);
                csPendingAction_ = 0; csPendingArg_.clear();
                if      (action == 1) loadFile(arg);
                else if (action == 2) doNew(arg);
            };
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                ICON_FA_TRIANGLE_EXCLAMATION " \"%s\" has unsaved changes.", csFile_.c_str());
            if (ImGui::Button("Save & continue"))    resolve(true);
            ImGui::SameLine();
            if (ImGui::Button("Discard & continue")) resolve(false);
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { csPendingAction_ = 0; csPendingArg_.clear(); }
            ImGui::Separator();
            return;
        }

        const std::vector<std::string> files = ListCutscenes(projectPath);

        ImGui::TextDisabled("File"); ImGui::SameLine();
        int fileIdx = -1;
        for (int i = 0; i < (int)files.size(); ++i) if (files[i] == csFile_) { fileIdx = i; break; }
        std::vector<const char*> fptrs; fptrs.reserve(files.size());
        for (const auto& s : files) fptrs.push_back(s.c_str());
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::Combo("##csfile", &fileIdx, fptrs.data(), (int)fptrs.size()) && fileIdx >= 0)
            requestLoad(files[fileIdx]);

        ImGui::SameLine();
        if (ImGui::SmallButton("Reload") && !csFile_.empty()) requestLoad(csFile_);
        ImGui::SameLine();
        {
            std::string saveLbl = ICON_FA_FLOPPY_DISK "  Save";
            if (csDirty_) saveLbl += " *";
            if (ImGui::SmallButton(saveLbl.c_str()) && csLoaded_ && !csFile_.empty()) saveFile(csFile_);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Undo") ) doUndo();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputTextWithHint("##csnew", "new name", csNewNameBuf_, sizeof(csNewNameBuf_));
        ImGui::SameLine();
        std::string newStem = csNewNameBuf_;
        newStem.erase(std::remove(newStem.begin(), newStem.end(), ' '), newStem.end());
        const bool newExists = !newStem.empty() && std::find(files.begin(), files.end(), newStem) != files.end();
        if (ImGui::SmallButton("New") && !newStem.empty() && !newExists)
        {
            requestNew(newStem);
            csNewNameBuf_[0] = '\0';
        }
        if (newExists) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "name exists"); }

        if (!csLoaded_)
        {
            ImGui::Spacing();
            ImGui::TextWrapped("Pick a cutscene from the File dropdown, or type a name and click New to "
                               "start cutscenes/<name>.json. Saved files run at runtime via "
                               "CutsceneManager::Get().Play(\"<name>\") or an Interactable's Cutscene Id.");
            if (files.empty()) ImGui::TextDisabled("(This project has no cutscene files yet.)");
            return;
        }

        // Tell Update()'s preview watchdog the panel is alive this frame, so a scrub
        // preview persists while this tab is visible and restores the scene when it isn't.
        csPreviewKeepAlive_ = true;

        ImGui::Separator();

        // --- cutscene settings ----------------------------------------------------
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::DragFloat("Duration (s)", &csDoc_.duration, 0.05f, 0.1f, 100000.0f, "%.2f"))
            csDirty_ = true;
        if (ImGui::IsItemActivated()) pushUndo();
        ImGui::SameLine();
        undoCheckbox("Loop", csDoc_.loop);
        ImGui::SameLine();
        undoCheckbox("Pause gameplay", csDoc_.pausesGameplay);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Freezes the scene (Time::scale=0) while the cutscene plays.");
        ImGui::SameLine();
        undoCheckbox("Letterbox", csDoc_.letterbox);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Draw cinematic black bars while the cutscene plays.");

        ImGui::SetNextItemWidth(150.0f);
        if (StrField("On-complete flag", csDoc_.onCompleteFlag)) csDirty_ = true;
        if (ImGui::IsItemActivated()) pushUndo();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (StrField("On-complete event", csDoc_.onCompleteEvent)) csDirty_ = true;
        if (ImGui::IsItemActivated()) pushUndo();

        // --- transport + add track ------------------------------------------------
        ImGui::Spacing();
        const bool playing = (state == GameState::Play);
        if (playing)
        {
            // Full-fidelity playback through the runtime (fires triggers, honors
            // pausesGameplay). Play is inherently non-destructive — the scene snapshot is
            // restored on the editor's global Stop.
            if (ImGui::SmallButton(ICON_FA_PLAY "##csplay"))
            {
                Cutscene c = csDoc_;
                for (auto& t : c.tracks) CutsceneManager::SortTrack(t);
                CutsceneManager::Get().PlayCutscene(c);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play this cutscene for real in the running game.");
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_STOP "##csstop")) CutsceneManager::Get().End();
            ImGui::SameLine();
            const bool act = CutsceneManager::Get().IsActive();
            ImGui::Text("%.2fs / %.2fs  %s", act ? CutsceneManager::Get().Time() : 0.0f, csDoc_.duration,
                        act ? "(playing)" : "");
        }
        else
        {
            // Non-destructive editor preview: interpolated tracks only, no triggers.
            if (ImGui::SmallButton(ICON_FA_BACKWARD_FAST "##csrew")) csPlayhead_ = 0.0f;
            ImGui::SameLine();
            if (!csPlaying_) { if (ImGui::SmallButton(ICON_FA_PLAY "##cspv"))  { csEnterPreview(); csPlaying_ = true; } }
            else             { if (ImGui::SmallButton(ICON_FA_PAUSE "##cspv")) { csPlaying_ = false; } }
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_STOP "##cspvstop")) { csExitPreview(); csPlayhead_ = 0.0f; }
            ImGui::SameLine();
            ImGui::Text("%.2fs / %.2fs", csPlayhead_, csDoc_.duration);
            if (csPreviewActive_)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " PREVIEW");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Entities are being driven for preview. Press Stop to restore the scene.");
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_PLUS " Add Track")) ImGui::OpenPopup("AddCutsceneTrack");
        ImGui::SameLine();
        ImGui::TextDisabled("zoom");
        ImGui::SameLine();
        if (ImGui::SmallButton("-##zoomout")) csPxPerSec_ = std::max(10.0f, csPxPerSec_ * 0.8f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##zoomin"))  csPxPerSec_ = std::min(1000.0f, csPxPerSec_ * 1.25f);

        // Add-track popup: choose a type, then (if needed) a target entity.
        if (ImGui::BeginPopup("AddCutsceneTrack"))
        {
            static int   newType = 0;
            static char  newTarget[96] = {};
            ImGui::TextDisabled("New track");
            const char* typeNames[] = { "Transform", "Camera", "Dialogue", "Audio", "Animation",
                                        "Activation", "StoryFlag", "Event", "Particle" };
            ImGui::SetNextItemWidth(160.0f);
            ImGui::Combo("Type", &newType, typeNames, IM_ARRAYSIZE(typeNames));

            const CutsceneTrackType nt = kAllTypes[newType];
            const bool wantsTarget = needsEntityTarget(nt) || nt == CutsceneTrackType::Camera;
            if (wantsTarget)
            {
                ImGui::SetNextItemWidth(160.0f);
                if (ImGui::BeginCombo("Target", newTarget[0] ? newTarget : (nt == CutsceneTrackType::Camera ? "(primary camera)" : "(choose entity)")))
                {
                    if (nt == CutsceneTrackType::Camera && ImGui::Selectable("(primary camera)", newTarget[0] == '\0'))
                        newTarget[0] = '\0';
                    for (const auto& e : scene.entities)
                    {
                        if (e->name.empty()) continue;
                        if (ImGui::Selectable(e->name.c_str(), e->name == newTarget))
                            std::strncpy(newTarget, e->name.c_str(), sizeof(newTarget) - 1);
                    }
                    ImGui::EndCombo();
                }
            }
            if (ImGui::Button("Add"))
            {
                pushUndo();
                CutsceneTrack t;
                t.type   = nt;
                t.target = wantsTarget ? std::string(newTarget) : std::string();
                if (nt == CutsceneTrackType::Transform) t.animatePosition = true;
                csDoc_.tracks.push_back(std::move(t));
                csDirty_ = true;
                newTarget[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (csDoc_.tracks.empty())
        {
            ImGui::Spacing();
            ImGui::TextDisabled("No tracks yet — click \"+ Add Track\" to start. Transform/Camera tracks hold "
                                "keyframes; Dialogue/Audio/etc. tracks fire events at points in time.");
            return;
        }

        // --- timeline (ruler + lanes) ---------------------------------------------
        const float headerW = 178.0f;
        const float rulerH  = 22.0f;
        const float trackH  = 36.0f;

        const float childH = std::min(360.0f, rulerH + trackH * csDoc_.tracks.size() + 8.0f);
        ImGui::BeginChild("##cstimeline", ImVec2(0, childH), true, ImGuiWindowFlags_NoScrollbar);

        ImDrawList* dl  = ImGui::GetWindowDrawList();
        const ImVec2 org = ImGui::GetCursorScreenPos();
        const float  fullW   = ImGui::GetContentRegionAvail().x;
        const float  lanesX0 = org.x + headerW;
        const float  lanesW  = std::max(60.0f, fullW - headerW);
        const float  lanesX1 = lanesX0 + lanesW;

        // Wheel over the timeline = zoom (centered on the cursor's time).
        if (ImGui::IsWindowHovered())
        {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                const float mx = ImGui::GetIO().MousePos.x;
                const float tAtMouse = csScrollSec_ + (mx - lanesX0) / csPxPerSec_;
                csPxPerSec_ = std::clamp(csPxPerSec_ * (wheel > 0 ? 1.15f : 0.87f), 10.0f, 1000.0f);
                csScrollSec_ = std::max(0.0f, tAtMouse - (mx - lanesX0) / csPxPerSec_);
            }
        }

        auto timeToX = [&](float t) { return lanesX0 + (t - csScrollSec_) * csPxPerSec_; };
        auto xToTime = [&](float x) { return csScrollSec_ + (x - lanesX0) / csPxPerSec_; };

        // Ruler: second ticks + labels, clipped to the lanes column.
        dl->PushClipRect(ImVec2(lanesX0, org.y), ImVec2(lanesX1, org.y + childH), true);
        dl->AddRectFilled(ImVec2(lanesX0, org.y), ImVec2(lanesX1, org.y + rulerH), IM_COL32(28, 28, 34, 255));
        {
            const float tStart = std::floor(csScrollSec_);
            const float tEnd   = xToTime(lanesX1);
            for (float t = tStart; t <= tEnd + 1.0f; t += 1.0f)
            {
                if (t < 0.0f) continue;
                const float x = timeToX(t);
                dl->AddLine(ImVec2(x, org.y), ImVec2(x, org.y + rulerH), IM_COL32(80, 80, 95, 255));
                char lbl[16]; std::snprintf(lbl, sizeof(lbl), "%gs", t);
                dl->AddText(ImVec2(x + 3, org.y + 3), IM_COL32(150, 150, 165, 255), lbl);
            }
            // End-of-duration marker
            const float dx = timeToX(csDoc_.duration);
            dl->AddLine(ImVec2(dx, org.y), ImVec2(dx, org.y + childH), IM_COL32(200, 80, 80, 160), 1.5f);
        }
        dl->PopClipRect();

        // Ruler scrub: click/drag in the ruler strip moves the playhead.
        ImGui::SetCursorScreenPos(ImVec2(lanesX0, org.y));
        ImGui::InvisibleButton("##csruler", ImVec2(lanesW, rulerH));
        if (ImGui::IsItemActive())
        {
            csPlayhead_ = std::clamp(xToTime(ImGui::GetIO().MousePos.x), 0.0f, csDoc_.duration);
            // Scrubbing in the editor drives a non-destructive preview (Update samples it);
            // in Play the runtime owns the entities, so just move the marker.
            if (state == GameState::Editor) { if (!csPreviewActive_) csEnterPreview(); csPlaying_ = false; }
        }

        // Tracks.
        int deleteTrack = -1;
        for (int ti = 0; ti < (int)csDoc_.tracks.size(); ++ti)
        {
            CutsceneTrack& tr = csDoc_.tracks[ti];
            ImGui::PushID(ti);

            const float rowY = org.y + rulerH + ti * trackH;
            const bool  rowSelected = (ti == csSelTrack_);

            // Lane background.
            dl->AddRectFilled(ImVec2(lanesX0, rowY), ImVec2(lanesX1, rowY + trackH - 2.0f),
                              rowSelected ? IM_COL32(48, 48, 60, 255) : IM_COL32(36, 36, 44, 255));
            dl->AddLine(ImVec2(org.x, rowY + trackH - 1.0f), ImVec2(lanesX1, rowY + trackH - 1.0f), IM_COL32(20, 20, 24, 255));

            // --- left header widgets ---
            ImGui::SetCursorScreenPos(ImVec2(org.x + 4, rowY + 3));
            dl->AddRectFilled(ImVec2(org.x + 2, rowY + 3), ImVec2(org.x + 6, rowY + trackH - 5), trackColor(tr.type));
            ImGui::SetCursorScreenPos(ImVec2(org.x + 10, rowY + 4));
            ImGui::TextColored(ImColor(trackColor(tr.type)), "%s", CutsceneManager::TrackTypeToStr(tr.type));

            ImGui::SetCursorScreenPos(ImVec2(org.x + 10, rowY + 18));
            const bool wantsTarget = needsEntityTarget(tr.type) || tr.type == CutsceneTrackType::Camera;
            if (wantsTarget)
            {
                ImGui::SetNextItemWidth(headerW - 60.0f);
                const char* preview = tr.target.empty()
                                    ? (tr.type == CutsceneTrackType::Camera ? "(primary camera)" : "(unbound)")
                                    : tr.target.c_str();
                if (ImGui::BeginCombo("##tgt", preview, ImGuiComboFlags_HeightSmall))
                {
                    if (tr.type == CutsceneTrackType::Camera && ImGui::Selectable("(primary camera)", tr.target.empty()))
                    { pushUndo(); tr.target.clear(); csDirty_ = true; }
                    for (const auto& e : scene.entities)
                    {
                        if (e->name.empty()) continue;
                        if (ImGui::Selectable(e->name.c_str(), e->name == tr.target))
                        { pushUndo(); tr.target = e->name; csDirty_ = true; }
                    }
                    ImGui::EndCombo();
                }
                // Warn when a bound name resolves to nothing in the current scene.
                if (!tr.target.empty())
                {
                    bool found = false;
                    for (const auto& e : scene.entities) if (e->name == tr.target) { found = true; break; }
                    if (!found)
                    {
                        ImGui::SameLine(0, 4);
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("No entity named \"%s\" in this scene.", tr.target.c_str());
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("(global)");
            }

            // mute + delete (right edge of header)
            ImGui::SetCursorScreenPos(ImVec2(org.x + headerW - 44.0f, rowY + 4));
            if (ImGui::SmallButton(tr.muted ? ICON_FA_VOLUME_XMARK : ICON_FA_VOLUME_HIGH)) { pushUndo(); tr.muted = !tr.muted; csDirty_ = true; }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(tr.muted ? "Muted" : "Active");
            ImGui::SetCursorScreenPos(ImVec2(org.x + headerW - 22.0f, rowY + 4));
            if (ImGui::SmallButton(ICON_FA_TRASH "##deltrk")) deleteTrack = ti;

            // Empty-lane double-click adds an item at that time. Submitted before the
            // per-key buttons below, so dragging an existing key still wins the hit-test.
            ImGui::SetCursorScreenPos(ImVec2(lanesX0, rowY));
            ImGui::InvisibleButton("lane", ImVec2(lanesW, trackH - 2.0f));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                addItemAt(tr, ti, std::clamp(xToTime(ImGui::GetIO().MousePos.x), 0.0f, csDoc_.duration));

            // --- lane items (clipped to the lanes column) ---
            dl->PushClipRect(ImVec2(lanesX0, rowY), ImVec2(lanesX1, rowY + trackH), true);
            const float laneCy = rowY + trackH * 0.5f;
            const ImU32 col = tr.muted ? (trackColor(tr.type) & 0x66FFFFFF) : trackColor(tr.type);

            if (tr.isInterpolated())
            {
                // Connector polyline in time order (temp index sort; vector stays put).
                std::vector<int> order(tr.keys.size());
                for (int i = 0; i < (int)order.size(); ++i) order[i] = i;
                std::sort(order.begin(), order.end(), [&](int a, int b) { return tr.keys[a].time < tr.keys[b].time; });
                for (int i = 0; i + 1 < (int)order.size(); ++i)
                    dl->AddLine(ImVec2(timeToX(tr.keys[order[i]].time), laneCy),
                                ImVec2(timeToX(tr.keys[order[i+1]].time), laneCy),
                                IM_COL32(160, 160, 180, 140), 1.5f);

                for (int ki = 0; ki < (int)tr.keys.size(); ++ki)
                {
                    const float kx = timeToX(tr.keys[ki].time);
                    ImGui::SetCursorScreenPos(ImVec2(kx - 7.0f, laneCy - 7.0f));
                    ImGui::PushID(ki);
                    ImGui::InvisibleButton("k", ImVec2(14.0f, 14.0f));
                    if (ImGui::IsItemActivated()) { csSelTrack_ = ti; csSelItem_ = ki; csDragMoved_ = false; }
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        if (!csDragMoved_) { pushUndo(); csDragMoved_ = true; }
                        tr.keys[ki].time = std::clamp(xToTime(ImGui::GetIO().MousePos.x), 0.0f, csDoc_.duration);
                        csDirty_ = true;
                    }
                    const bool sel = (ti == csSelTrack_ && ki == csSelItem_);
                    const float r = sel ? 7.0f : 5.5f;
                    const ImVec2 d[4] = { {kx, laneCy - r}, {kx + r, laneCy}, {kx, laneCy + r}, {kx - r, laneCy} };
                    ImU32 fill = sel ? IM_COL32(255, 255, 255, 255) : col;
                    dl->AddConvexPolyFilled(d, 4, fill);
                    dl->AddPolyline(d, 4, IM_COL32(20, 20, 24, 255), ImDrawFlags_Closed, 1.0f);
                    ImGui::PopID();
                }
            }
            else
            {
                for (int ei = 0; ei < (int)tr.events.size(); ++ei)
                {
                    const float ex = timeToX(tr.events[ei].time);
                    ImGui::SetCursorScreenPos(ImVec2(ex - 5.0f, rowY + 6.0f));
                    ImGui::PushID(1000 + ei);
                    ImGui::InvisibleButton("e", ImVec2(12.0f, trackH - 12.0f));
                    if (ImGui::IsItemActivated()) { csSelTrack_ = ti; csSelItem_ = ei; csDragMoved_ = false; }
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        if (!csDragMoved_) { pushUndo(); csDragMoved_ = true; }
                        tr.events[ei].time = std::clamp(xToTime(ImGui::GetIO().MousePos.x), 0.0f, csDoc_.duration);
                        csDirty_ = true;
                    }
                    const bool sel = (ti == csSelTrack_ && ei == csSelItem_);
                    dl->AddRectFilled(ImVec2(ex - 4.0f, rowY + 6.0f), ImVec2(ex + 4.0f, rowY + trackH - 8.0f),
                                      sel ? IM_COL32(255, 255, 255, 255) : col);
                    dl->AddLine(ImVec2(ex, rowY + 4.0f), ImVec2(ex, rowY + trackH - 4.0f), col, 1.0f);
                    const std::string& a = tr.events[ei].a;
                    if (!a.empty()) dl->AddText(ImVec2(ex + 7.0f, laneCy - 7.0f), IM_COL32(210, 210, 225, 255), a.c_str());
                    ImGui::PopID();
                }
            }
            dl->PopClipRect();

            ImGui::PopID();
        }

        // Playhead line over everything.
        {
            const float px = timeToX(csPlayhead_);
            if (px >= lanesX0 && px <= lanesX1)
            {
                dl->PushClipRect(ImVec2(lanesX0, org.y), ImVec2(lanesX1, org.y + childH), true);
                dl->AddLine(ImVec2(px, org.y), ImVec2(px, org.y + childH), IM_COL32(90, 200, 255, 255), 1.5f);
                dl->AddTriangleFilled(ImVec2(px - 5, org.y), ImVec2(px + 5, org.y), ImVec2(px, org.y + 7), IM_COL32(90, 200, 255, 255));
                dl->PopClipRect();
            }
        }

        ImGui::EndChild();

        if (deleteTrack >= 0)
        {
            pushUndo();
            csDoc_.tracks.erase(csDoc_.tracks.begin() + deleteTrack);
            if (csSelTrack_ == deleteTrack) { csSelTrack_ = csSelItem_ = -1; }
            else if (csSelTrack_ > deleteTrack) csSelTrack_--;
            csDirty_ = true;
        }

        // --- selected-item inspector ----------------------------------------------
        ImGui::Separator();
        if (csSelTrack_ >= 0 && csSelTrack_ < (int)csDoc_.tracks.size())
        {
            CutsceneTrack& tr = csDoc_.tracks[csSelTrack_];

            if (tr.type == CutsceneTrackType::Transform)
            {
                ImGui::TextDisabled("Channels:"); ImGui::SameLine();
                undoCheckbox("Pos",   tr.animatePosition); ImGui::SameLine();
                undoCheckbox("Rot",   tr.animateRotation); ImGui::SameLine();
                undoCheckbox("Scale", tr.animateScale);
            }

            const bool interp = tr.isInterpolated();
            const int  count  = interp ? (int)tr.keys.size() : (int)tr.events.size();

            if (ImGui::SmallButton(interp ? "+ Add Key at playhead" : "+ Add Event at playhead"))
                addItemAt(tr, csSelTrack_, csPlayhead_);

            if (csSelItem_ >= 0 && csSelItem_ < count)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton(ICON_FA_TRASH " Delete selected"))
                {
                    pushUndo();
                    if (interp) tr.keys.erase(tr.keys.begin() + csSelItem_);
                    else        tr.events.erase(tr.events.begin() + csSelItem_);
                    csSelItem_ = -1;
                    csDirty_ = true;
                }

                if (csSelItem_ >= 0)
                {
                    ImGui::Spacing();
                    if (interp)
                    {
                        CutsceneKey& k = tr.keys[csSelItem_];
                        ImGui::SetNextItemWidth(90.0f);
                        if (ImGui::DragFloat("Time##k", &k.time, 0.02f, 0.0f, csDoc_.duration, "%.2f")) csDirty_ = true;
                        if (ImGui::IsItemActivated()) pushUndo();

                        if (tr.type == CutsceneTrackType::Transform)
                        {
                            if (tr.animatePosition) { ImGui::SetNextItemWidth(160.0f); if (ImGui::DragFloat2("Pos", &k.pos.x, 1.0f)) csDirty_ = true; if (ImGui::IsItemActivated()) pushUndo(); }
                            if (tr.animateRotation) { ImGui::SetNextItemWidth(90.0f);  if (ImGui::DragFloat("Rot", &k.rot, 0.5f, 0.0f, 0.0f, "%.1f")) csDirty_ = true; if (ImGui::IsItemActivated()) pushUndo(); }
                            if (tr.animateScale)    { ImGui::SetNextItemWidth(160.0f); if (ImGui::DragFloat2("Scale", &k.scale.x, 0.01f)) csDirty_ = true; if (ImGui::IsItemActivated()) pushUndo(); }
                        }
                        else // Camera
                        {
                            ImGui::SetNextItemWidth(160.0f); if (ImGui::DragFloat2("Look-at", &k.pos.x, 1.0f)) csDirty_ = true; if (ImGui::IsItemActivated()) pushUndo();
                            ImGui::SetNextItemWidth(90.0f);  if (ImGui::DragFloat("Zoom", &k.zoom, 0.01f, 0.05f, 20.0f, "%.2f")) csDirty_ = true; if (ImGui::IsItemActivated()) pushUndo();
                            ImGui::SetNextItemWidth(90.0f);  if (ImGui::DragFloat("Rot##cam", &k.rot, 0.5f, 0.0f, 0.0f, "%.1f")) csDirty_ = true; if (ImGui::IsItemActivated()) pushUndo();
                        }

                        const char* easeNames[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Step" };
                        int em = (int)k.easing;
                        ImGui::SetNextItemWidth(120.0f);
                        if (ImGui::Combo("Easing", &em, easeNames, IM_ARRAYSIZE(easeNames))) { pushUndo(); k.easing = (CutsceneEasing)em; csDirty_ = true; }
                    }
                    else
                    {
                        CutsceneEvent& ev = tr.events[csSelItem_];
                        ImGui::SetNextItemWidth(90.0f);
                        if (ImGui::DragFloat("Time##e", &ev.time, 0.02f, 0.0f, csDoc_.duration, "%.2f")) csDirty_ = true;
                        if (ImGui::IsItemActivated()) pushUndo();
                        ImGui::SetNextItemWidth(180.0f);
                        if (StrField("a", ev.a)) csDirty_ = true;
                        if (ImGui::IsItemActivated()) pushUndo();
                        ImGui::SameLine(); ImGui::TextDisabled("(%s)", payloadHint(tr.type));
                        ImGui::SetNextItemWidth(180.0f);
                        if (StrField("b", ev.b)) csDirty_ = true;
                        if (ImGui::IsItemActivated()) pushUndo();
                        undoCheckbox("Fire on skip", ev.fireOnSkip);
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("Select a keyframe/event on the timeline, or add one at the playhead.");
            }
        }
        else
        {
            ImGui::TextDisabled("Select a track (click a keyframe) to edit it.");
        }
    }

    // ── Non-destructive editor preview ───────────────────────────────────────────
    // Snapshot the transforms of every entity an interpolated track drives, so leaving
    // preview can restore them. Camera tracks also remember zoom/rotation/follow.
    void Editor::csEnterPreview()
    {
        if (csPreviewActive_ || !csLoaded_) return;
        csPreviewSave_.clear();

        auto resolve = [&](const CutsceneTrack& tr) -> Entity*
        {
            if (tr.type == CutsceneTrackType::Camera && tr.target.empty())
            {
                for (const auto& e : scene.entities) if (e->getComponent<CameraComponent>()) return e.get();
                return nullptr;
            }
            for (const auto& e : scene.entities) if (e->name == tr.target) return e.get();
            return nullptr;
        };

        for (const auto& tr : csDoc_.tracks)
        {
            if (!tr.isInterpolated()) continue;
            Entity* e = resolve(tr);
            if (!e) continue;
            bool dup = false;
            for (const auto& s : csPreviewSave_) if (s.id == e->id) { dup = true; break; }
            if (dup) continue;

            CsPreviewSave s{};
            s.id = e->id; s.pos = e->position; s.rot = e->rotation; s.scale = e->scale;
            if (auto* cam = e->getComponent<CameraComponent>())
            { s.isCam = true; s.zoom = cam->zoom; s.camRot = cam->baseRotation; s.follow = cam->followEnabled; }
            csPreviewSave_.push_back(s);
        }
        csPreviewActive_ = true;
    }

    void Editor::csExitPreview()
    {
        if (!csPreviewActive_) return;
        for (const auto& s : csPreviewSave_)
        {
            Entity* e = scene.FindEntity(s.id);
            if (!e) continue;
            e->position = s.pos; e->rotation = s.rot; e->scale = s.scale;
            if (s.isCam)
                if (auto* cam = e->getComponent<CameraComponent>())
                { cam->SetZoom(s.zoom); cam->baseRotation = s.camRot; cam->followEnabled = s.follow; }
        }
        csPreviewSave_.clear();
        csPreviewActive_ = false;
        csPlaying_       = false;
    }

    void Editor::csSamplePreview()
    {
        if (!csLoaded_) return;
        CutsceneManager::SampleCutscene(csDoc_, csPlayhead_, &scene);
    }
}

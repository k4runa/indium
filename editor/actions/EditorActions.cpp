#include "../Editor.hpp"

namespace Indium
{
    void Editor::DeleteEntity(Entity& entity)
    {
        std::function<void(Entity&)> doDelete = [&](Entity& ent)
        {
            std::vector<Entity*> childrenCopy = ent.children;
            for (Entity* child : childrenCopy) { doDelete(*child); }

            auto it = std::find_if(scene.entities.begin(), scene.entities.end(),
                [&](const std::unique_ptr<Entity>& e) { return e.get() == &ent; });

            if (it != scene.entities.end())
            {
                for (auto& comp : ent.components) comp->destroy(&scene);

                if (ent.parent)
                {
                    auto& sibs = ent.parent->children;
                    sibs.erase(std::remove(sibs.begin(), sibs.end(), &ent), sibs.end());
                }
                scene.entities.erase(it);
            }
        };

        doDelete(entity);
        selectedIndex = -1;
    }

    void Editor::DeleteEntitiesAt(const std::vector<int>& indices)
    {
        // Resolve indices to stable entity IDs before deleting anything. DeleteEntity
        // cascades over children and erases from scene.entities, which invalidates both
        // positional indices and any entity pointers held across iterations. Deleting in
        // descending-index order is NOT enough: a cascade can remove an entity at a lower
        // index than a still-pending one (or shrink the vector past it), so the old loop
        // could delete the wrong entity or index out of bounds. Each ID is deleted only
        // if it still exists — a parent's cascade may already have removed a descendant.
        std::vector<int> ids;
        ids.reserve(indices.size());
        for (int i : indices)
        {
            if (i >= 0 && i < (int)scene.entities.size()) ids.push_back(scene.entities[i]->id);
        }

        for (int id : ids)
        {
            if (Entity* ent = scene.FindEntity(id)) DeleteEntity(*ent);
        }
    }

    void Editor::TakeSnapshot()
    {
        if (state == GameState::Play) return;

        undoStack.push_back(scene.serialize());
        if (undoStack.size() > MaxUndoSteps) { undoStack.pop_front(); }
        redoStack.clear();
        isDirty = true;
    }

    void Editor::ApplyHistoryState(std::deque<nlohmann::json>& from, std::deque<nlohmann::json>& to)
    {
        if (from.empty() || state == GameState::Play) return;
        to.push_back(scene.serialize());
        nlohmann::json snapshot = from.back();
        from.pop_back();

        scene.entities.clear();
        scene.nextEntityId = snapshot.contains("nextEntityId") ? snapshot["nextEntityId"].get<int>() : 1;

        if (snapshot.contains("worldSize"))
        {
            scene.worldSize.x = snapshot["worldSize"][0].get<float>();
            scene.worldSize.y = snapshot["worldSize"][1].get<float>();
        }

        if (snapshot.contains("editorCamera"))
        {
            editorCamera.target.x = snapshot["editorCamera"][0].get<float>();
            editorCamera.target.y = snapshot["editorCamera"][1].get<float>();
            editorCamera.zoom     = snapshot["editorCamera"][2].get<float>();
        }

        if (snapshot.contains("entities"))
        {
            for (const auto& ej : snapshot["entities"])
            {
                auto entity = factory.LoadEntity(ej);
                if (entity) scene.entities.push_back(std::move(entity));
            }
            scene.RebuildHierarchy();
        }
        scene.storyState = snapshot.contains("storyState") ? StoryValueMapFromJson(snapshot["storyState"]) : std::map<std::string, StoryValue>{};

        selectedIndex = -1;
        multiSelection_.clear();
    }

    void Editor::Undo() { ApplyHistoryState(undoStack, redoStack); }
    void Editor::Redo() { ApplyHistoryState(redoStack, undoStack); }

    void Editor::CopySelected()
    {
        if (multiSelection_.size() > 1)
        {
            multiClipboard_.clear();
            for (int i : multiSelection_) multiClipboard_.push_back(scene.entities[i]->serialize());
            entityClipboard = {};
        }
        else if (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
        {
            entityClipboard = scene.entities[selectedIndex]->serialize();
            multiClipboard_.clear();
        }
    }

    void Editor::PasteAt(Vector2 pos)
    {
        TakeSnapshot();
        if (!multiClipboard_.empty())
        {
            for (auto& j : multiClipboard_)
            {
                auto pasted = factory.LoadEntity(j);
                if (!pasted) continue;
                pasted->id       = scene.nextEntityId++;
                pasted->parentId = -1;
                pasted->position = Vector2Add(pasted->position, {16.0f, 16.0f});
                scene.entities.push_back(std::move(pasted));
            }
            selectedIndex = (int)scene.entities.size() - 1;
        }
        else if (!entityClipboard.is_null())
        {
            auto pasted = factory.LoadEntity(entityClipboard);
            if (!pasted) return;
            pasted->id       = scene.nextEntityId++;
            pasted->parentId = -1;
            pasted->position = pos;
            scene.entities.push_back(std::move(pasted));
            selectedIndex = (int)scene.entities.size() - 1;
        }
    }

    void Editor::DuplicateSelected(int index)
    {
        TakeSnapshot();
        if (multiSelection_.size() > 1)
        {
            std::vector<nlohmann::json> todupJ;
            for (int i : multiSelection_) todupJ.push_back(scene.entities[i]->serialize());
            for (auto& j : todupJ)
            {
                auto dup = factory.LoadEntity(j);
                if (dup)
                {
                    dup->id       = scene.nextEntityId++;
                    dup->name    += " (Copy)";
                    dup->position = Vector2Add(dup->position, {16.0f, 16.0f});
                    scene.entities.push_back(std::move(dup));
                }
            }
            selectedIndex = (int)scene.entities.size() - 1;
        }
        else if (index >= 0 && index < (int)scene.entities.size())
        {
            Entity* ent = scene.entities[index].get();
            auto dup = factory.LoadEntity(ent->serialize());
            if (dup)
            {
                dup->id       = scene.nextEntityId++;
                dup->name     = ent->name + " (Copy)";
                dup->position = Vector2Add(dup->position, {16.0f, 16.0f});
                scene.entities.push_back(std::move(dup));
                scene.entities.back()->setParent(ent->parent);
                selectedIndex = (int)scene.entities.size() - 1;
            }
        }
    }
}

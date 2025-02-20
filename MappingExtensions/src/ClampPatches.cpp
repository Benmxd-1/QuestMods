#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/BeatmapData_-get_beatmapObjectsData-d__31.hpp"
#include "GlobalNamespace/BeatmapLineData.hpp"
#include "GlobalNamespace/BeatmapObjectData.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/NotesInTimeRowProcessor.hpp"

#include <map>

using namespace GlobalNamespace;

int addBeatmapObjectDataLineIndex;
MAKE_HOOK_MATCH(BeatmapData_AddBeatmapObjectData, &BeatmapData::AddBeatmapObjectData, void,
                BeatmapData *self, BeatmapObjectData *item) {
    addBeatmapObjectDataLineIndex = item->lineIndex;
    // Preprocess the lineIndex to be 0-3 (the real method is hard-coded to 4
    // lines), recording the info needed to reverse it
    if (addBeatmapObjectDataLineIndex > 3) {
        item->lineIndex = 3;
    } else if (addBeatmapObjectDataLineIndex < 0) {
        item->lineIndex = 0;
    }

    BeatmapData_AddBeatmapObjectData(self, item);
}
MAKE_HOOK_MATCH(BeatmapLineData_AddBeatmapObjectData, &BeatmapLineData::AddBeatmapObjectData, void,
                BeatmapLineData *self, BeatmapObjectData *item) {
    item->lineIndex = addBeatmapObjectDataLineIndex;
    BeatmapLineData_AddBeatmapObjectData(self, item);
}

MAKE_HOOK_MATCH(NoteProcessorClampPatch, &NotesInTimeRowProcessor::ProcessAllNotesInTimeRow, void,
                NotesInTimeRowProcessor *self, List<NoteData *> *notes) {
    std::map<int, int> extendedLanesMap;
    for (int i = 0; i < notes->size; ++i) {
        auto *item = notes->items->values[i];
        if (item->lineIndex > 3) {
            extendedLanesMap[i] = item->lineIndex;
            item->lineIndex = 3;
        } else if (item->lineIndex < 0) {
            extendedLanesMap[i] = item->lineIndex;
            item->lineIndex = 0;
        }
    }

    // NotesInTimeRowProcessor_ProcessAllNotesInTimeRow(self, notes);
    // Instead, we have a reimplementation of the hooked method to deal with precision
    // noteLineLayers:
    for (il2cpp_array_size_t i = 0; i < self->notesInColumns->Length(); i++) {
        self->notesInColumns->values[i]->Clear();
    }
    for (int j = 0; j < notes->size; j++) {
        auto *noteData = notes->items->values[j];
        auto *list = self->notesInColumns->values[noteData->lineIndex];

        bool flag = false;
        for (int k = 0; k < list->size; k++) {
            if (list->items->values[k]->noteLineLayer.value > noteData->noteLineLayer.value) {
                list->Insert(k, noteData);
                flag = true;
                break;
            }
        }
        if (!flag) {
            list->Add(noteData);
        }
    }
    for (il2cpp_array_size_t l = 0; l < self->notesInColumns->Length(); l++) {
        auto *list2 = self->notesInColumns->values[l];
        for (int m = 0; m < list2->size; m++) {
            auto *note = list2->items->values[m];
            if (note->noteLineLayer.value >= 0 && note->noteLineLayer.value <= 2) {
                note->SetNoteStartLineLayer(m);
            }
        }
    }

    for (int i = 0; i < notes->size; ++i) {
        if (extendedLanesMap.find(i) != extendedLanesMap.end()) {
            auto *item = notes->items->values[i];
            item->lineIndex = extendedLanesMap[i];
        }
    }
}

MAKE_HOOK_MATCH(BeatmapObjectsDataClampPatch, &BeatmapData::$get_beatmapObjectsData$d__31::MoveNext,
                bool, BeatmapData::$get_beatmapObjectsData$d__31 *self) {
    int num = self->$$1__state;
    BeatmapData *beatmapData = self->$$4__this;
    if (num != 0) {
        if (num != 1) {
            return false;
        }
        self->$$1__state = -1;
        // Increment index in idxs with clamped lineIndex
        int lineIndex = self->$minBeatmapObjectData$5__4->lineIndex;
        int clampedLineIndex = std::clamp(lineIndex, 0, 3);
        self->$idxs$5__3->values[clampedLineIndex]++;
        self->$minBeatmapObjectData$5__4 = nullptr;
    } else {
        self->$$1__state = -1;
        auto *arr =
            reinterpret_cast<Array<BeatmapLineData *> *>(beatmapData->get_beatmapLinesData());
        self->$beatmapLinesData$5__2 = arr;
        self->$idxs$5__3 = Array<int>::NewLength(self->$beatmapLinesData$5__2->Length());
    }
    self->$minBeatmapObjectData$5__4 = nullptr;
    float num2 = std::numeric_limits<float>::max();
    for (int i = 0; i < self->$beatmapLinesData$5__2->Length(); i++) {
        int idx = self->$idxs$5__3->values[i];
        BeatmapLineData *lineData = self->$beatmapLinesData$5__2->values[i];
        if (idx < lineData->beatmapObjectsData->get_Count()) {
            BeatmapObjectData *beatmapObjectData = lineData->beatmapObjectsData->get_Item(idx);
            float time = beatmapObjectData->time;
            if (time < num2) {
                num2 = time;
                self->$minBeatmapObjectData$5__4 = beatmapObjectData;
            }
        }
    }
    if (self->$minBeatmapObjectData$5__4 == nullptr) {
        return false;
    }
    self->$$2__current = self->$minBeatmapObjectData$5__4;
    self->$$1__state = 1;
    return true;
}

void InstallClampPatches(Logger &logger) {
    INSTALL_HOOK(logger, BeatmapObjectsDataClampPatch);
    INSTALL_HOOK(logger, NoteProcessorClampPatch);
    INSTALL_HOOK(logger, BeatmapData_AddBeatmapObjectData);
    INSTALL_HOOK(logger, BeatmapLineData_AddBeatmapObjectData);
}
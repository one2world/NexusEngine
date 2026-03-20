#include <gtest/gtest.h>
#include "nexus/editor/undo_redo.h"

using namespace nexus;
using namespace nexus::editor;

// =============================================================================
// LambdaCommand
// =============================================================================

TEST(LambdaCommand, ExecuteAndUndo) {
    int value = 0;
    LambdaCommand cmd("Set to 5",
        [&]() { value = 5; },
        [&]() { value = 0; });

    cmd.execute();
    EXPECT_EQ(value, 5);

    cmd.undo();
    EXPECT_EQ(value, 0);
}

TEST(LambdaCommand, Description) {
    LambdaCommand cmd("Test", [](){}, [](){});
    EXPECT_EQ(cmd.description(), "Test");
}

TEST(LambdaCommand, TypeId) {
    LambdaCommand cmd("", [](){}, [](){});
    EXPECT_STREQ(cmd.type_id(), "LambdaCommand");
}

// =============================================================================
// CompoundCommand
// =============================================================================

TEST(CompoundCommand, ExecutesAll) {
    int a = 0, b = 0;
    auto cmd = std::make_unique<CompoundCommand>("batch");
    cmd->add(std::make_unique<LambdaCommand>("a", [&](){ a = 1; }, [&](){ a = 0; }));
    cmd->add(std::make_unique<LambdaCommand>("b", [&](){ b = 2; }, [&](){ b = 0; }));

    cmd->execute();
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
}

TEST(CompoundCommand, UndoInReverseOrder) {
    std::vector<int> order;
    auto cmd = std::make_unique<CompoundCommand>("batch");
    cmd->add(std::make_unique<LambdaCommand>("1",
        [&](){ order.push_back(1); },
        [&](){ order.push_back(-1); }));
    cmd->add(std::make_unique<LambdaCommand>("2",
        [&](){ order.push_back(2); },
        [&](){ order.push_back(-2); }));

    cmd->execute();
    order.clear();
    cmd->undo();

    EXPECT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], -2); // Second undone first
    EXPECT_EQ(order[1], -1);
}

TEST(CompoundCommand, SubCommandCount) {
    auto cmd = std::make_unique<CompoundCommand>("batch");
    cmd->add(std::make_unique<LambdaCommand>("a", [](){}, [](){}));
    cmd->add(std::make_unique<LambdaCommand>("b", [](){}, [](){}));
    EXPECT_EQ(cmd->sub_command_count(), 2u);
}

// =============================================================================
// UndoRedoManager
// =============================================================================

TEST(UndoRedoManager, ExecuteAddsToStack) {
    UndoRedoManager mgr;
    EXPECT_FALSE(mgr.can_undo());

    int val = 0;
    mgr.execute(std::make_unique<LambdaCommand>("set",
        [&](){ val = 1; }, [&](){ val = 0; }));

    EXPECT_EQ(val, 1);
    EXPECT_TRUE(mgr.can_undo());
    EXPECT_EQ(mgr.undo_count(), 1u);
}

TEST(UndoRedoManager, Undo) {
    UndoRedoManager mgr;
    int val = 0;
    mgr.execute(std::make_unique<LambdaCommand>("set",
        [&](){ val = 1; }, [&](){ val = 0; }));

    EXPECT_TRUE(mgr.undo());
    EXPECT_EQ(val, 0);
    EXPECT_FALSE(mgr.can_undo());
    EXPECT_TRUE(mgr.can_redo());
}

TEST(UndoRedoManager, Redo) {
    UndoRedoManager mgr;
    int val = 0;
    mgr.execute(std::make_unique<LambdaCommand>("set",
        [&](){ val = 1; }, [&](){ val = 0; }));
    mgr.undo();

    EXPECT_TRUE(mgr.redo());
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(mgr.can_undo());
    EXPECT_FALSE(mgr.can_redo());
}

TEST(UndoRedoManager, NewCommandClearsRedoStack) {
    UndoRedoManager mgr;
    int val = 0;
    mgr.execute(std::make_unique<LambdaCommand>("a",
        [&](){ val = 1; }, [&](){ val = 0; }));
    mgr.undo();
    EXPECT_TRUE(mgr.can_redo());

    mgr.execute(std::make_unique<LambdaCommand>("b",
        [&](){ val = 2; }, [&](){ val = 0; }));
    EXPECT_FALSE(mgr.can_redo()); // Redo cleared
}

TEST(UndoRedoManager, MultipleUndoRedo) {
    UndoRedoManager mgr;
    int val = 0;
    mgr.execute(std::make_unique<LambdaCommand>("1",
        [&](){ val = 1; }, [&](){ val = 0; }));
    mgr.execute(std::make_unique<LambdaCommand>("2",
        [&](){ val = 2; }, [&](){ val = 1; }));
    mgr.execute(std::make_unique<LambdaCommand>("3",
        [&](){ val = 3; }, [&](){ val = 2; }));

    EXPECT_EQ(val, 3);
    mgr.undo(); EXPECT_EQ(val, 2);
    mgr.undo(); EXPECT_EQ(val, 1);
    mgr.redo(); EXPECT_EQ(val, 2);
}

TEST(UndoRedoManager, UndoDescription) {
    UndoRedoManager mgr;
    EXPECT_EQ(mgr.undo_description(), "");

    mgr.execute(std::make_unique<LambdaCommand>("Move entity",
        [](){}, [](){}));
    EXPECT_EQ(mgr.undo_description(), "Move entity");
}

TEST(UndoRedoManager, RedoDescription) {
    UndoRedoManager mgr;
    mgr.execute(std::make_unique<LambdaCommand>("Move entity",
        [](){}, [](){}));
    mgr.undo();
    EXPECT_EQ(mgr.redo_description(), "Move entity");
}

TEST(UndoRedoManager, History) {
    UndoRedoManager mgr;
    mgr.execute(std::make_unique<LambdaCommand>("A", [](){}, [](){}));
    mgr.execute(std::make_unique<LambdaCommand>("B", [](){}, [](){}));
    mgr.execute(std::make_unique<LambdaCommand>("C", [](){}, [](){}));

    auto history = mgr.undo_history();
    EXPECT_EQ(history.size(), 3u);
    EXPECT_EQ(history[0], "C"); // Most recent first
    EXPECT_EQ(history[1], "B");
    EXPECT_EQ(history[2], "A");
}

TEST(UndoRedoManager, MaxHistory) {
    UndoRedoManager mgr;
    mgr.set_max_history(3);

    for (int i = 0; i < 5; i++) {
        mgr.execute(std::make_unique<LambdaCommand>(
            std::to_string(i), [](){}, [](){}));
    }

    EXPECT_EQ(mgr.undo_count(), 3u);
}

TEST(UndoRedoManager, Clear) {
    UndoRedoManager mgr;
    mgr.execute(std::make_unique<LambdaCommand>("A", [](){}, [](){}));
    mgr.undo();
    mgr.clear();

    EXPECT_FALSE(mgr.can_undo());
    EXPECT_FALSE(mgr.can_redo());
}

TEST(UndoRedoManager, DirtyTracking) {
    UndoRedoManager mgr;
    EXPECT_FALSE(mgr.is_dirty());

    mgr.execute(std::make_unique<LambdaCommand>("A", [](){}, [](){}));
    EXPECT_TRUE(mgr.is_dirty());

    mgr.mark_saved();
    EXPECT_FALSE(mgr.is_dirty());

    mgr.execute(std::make_unique<LambdaCommand>("B", [](){}, [](){}));
    EXPECT_TRUE(mgr.is_dirty());

    mgr.undo();
    EXPECT_FALSE(mgr.is_dirty()); // Back to saved state
}

TEST(UndoRedoManager, UndoEmptyReturnsFalse) {
    UndoRedoManager mgr;
    EXPECT_FALSE(mgr.undo());
}

TEST(UndoRedoManager, RedoEmptyReturnsFalse) {
    UndoRedoManager mgr;
    EXPECT_FALSE(mgr.redo());
}

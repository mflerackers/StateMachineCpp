#include "machine.h"

/* Test cases */

void testPermit() {
    /*
        A   B
    */
    std::cout << "-- testPermit\n";
    std::string sequence;
    Machine<std::string, std::string> m("A");
    m.configure("A")
        .permit("X", "B")
        .onEntry([&sequence](){ std::cout << "entering A\n"; sequence += ">A"; })
        .onExit([&sequence](){ std::cout << "exiting A\n"; sequence += "<A"; });
    m.configure("B")
        .onEntry([&sequence](){ std::cout << "entering B\n"; sequence += ">B"; })
        .onExit([&sequence](){ std::cout << "exiting B\n"; sequence += "<B"; });
    assert(m.isInState("A"));
    m.fire("X");
    assert(m.isInState("B"));
    std::cout << sequence << "\n";
    assert(sequence == "<A>B");
}

void testInitialSubState() {
    /*
        A   B
            |
            C
    */
    std::cout << "-- testInitialSubState\n";
    std::string sequence;
    Machine<std::string, std::string> m("A");
    m.configure("A")
        .permit("X", "B")
        .onEntry([&sequence](){ std::cout << "entering A\n"; sequence += ">A"; })
        .onExit([&sequence](){ std::cout << "exiting A\n"; sequence += "<A"; });
    m.configure("B")
        .initialTransition("C")
        .onEntry([&sequence](){ std::cout << "entering B\n"; sequence += ">B"; })
        .onExit([&sequence](){ std::cout << "exiting B\n"; sequence += "<B"; });
    m.configure("C")
        .substateOf("B")
        .onEntry([&sequence](){ std::cout << "entering C\n"; sequence += ">C"; })
        .onExit([&sequence](){ std::cout << "exiting C\n"; sequence += "<C"; });
    assert(m.isInState("A"));
    m.fire("X");
    assert(m.isInState("C"));
    std::cout << sequence << "\n";
    assert(sequence == "<A>B>C");
}

void testExitSubState() {
    /*
        A   B
            |
            C
    */
    std::cout << "-- testExitSubState\n";
    std::string sequence;
    Machine<std::string, std::string> m("C");
    m.configure("A")
        .onEntry([&sequence](){ std::cout << "entering A\n"; sequence += ">A"; })
        .onExit([&sequence](){ std::cout << "exiting A\n"; sequence += "<A"; });
    m.configure("B")
        .onEntry([&sequence](){ std::cout << "entering B\n"; sequence += ">B"; })
        .onExit([&sequence](){ std::cout << "exiting B\n"; sequence += "<B"; });
    m.configure("C")
        .substateOf("B")
        .permit("X", "A")
        .onEntry([&sequence](){ std::cout << "entering C\n"; sequence += ">C"; })
        .onExit([&sequence](){ std::cout << "exiting C\n"; sequence += "<C"; });
    assert(m.isInState("C"));
    m.fire("X");
    assert(m.isInState("A"));
    std::cout << sequence << "\n";
    assert(sequence == "<C<B>A");
}

void testExitSiblingSubState() {
    /*
            B
           / \
          A   C
    */
    std::cout << "-- testExitSiblingSubState\n";
    std::string sequence;
    Machine<std::string, std::string> m("B");
    m.configure("A")
        .onEntry([&sequence](){ std::cout << "entering A\n"; sequence += ">A"; })
        .onExit([&sequence](){ std::cout << "exiting A\n"; sequence += "<A"; });
    m.configure("B")
        .substateOf("A")
        .permit("X", "C")
        .onEntry([&sequence](){ std::cout << "entering B\n"; sequence += ">B"; })
        .onExit([&sequence](){ std::cout << "exiting B\n"; sequence += "<B"; });
    m.configure("C")
        .substateOf("A")
        .onEntry([&sequence](){ std::cout << "entering C\n"; sequence += ">C"; })
        .onExit([&sequence](){ std::cout << "exiting C\n"; sequence += "<C"; });
    assert(m.isInState("B"));
    m.fire("X");
    assert(m.isInState("C"));
    std::cout << sequence << "\n";
    assert(sequence == "<B>C");
}

void testExitSuperSubState() {
    /*
            A
           / \
          B   C
              |
              D
    */
    std::cout << "-- testExitSuperSubState\n";
    std::string sequence;
    Machine<std::string, std::string> m("D");
    m.configure("A")
        .onEntry([&sequence](){ std::cout << "entering A\n"; sequence += ">A"; })
        .onExit([&sequence](){ std::cout << "exiting A\n"; sequence += "<A"; });
    m.configure("B")
        .substateOf("A")
        .onEntry([&sequence](){ std::cout << "entering B\n"; sequence += ">B"; })
        .onExit([&sequence](){ std::cout << "exiting B\n"; sequence += "<B"; });
    m.configure("C")
        .substateOf("A")
        .onEntry([&sequence](){ std::cout << "entering C\n"; sequence += ">C"; })
        .onExit([&sequence](){ std::cout << "exiting C\n"; sequence += "<C"; });
    m.configure("D")
        .substateOf("C")
        .permit("X", "B")
        .onEntry([&sequence](){ std::cout << "entering D\n"; sequence += ">D"; })
        .onExit([&sequence](){ std::cout << "exiting D\n"; sequence += "<D"; });
    assert(m.isInState("D"));
    m.fire("X");
    assert(m.isInState("B"));
    std::cout << sequence << "\n";
    assert(sequence == "<D<C>B");
}

void testEnterSuperSubState() {
    /*
            A
           / \
          B   C
              |
              D
    */
    std::cout << "-- testEnterSuperSubState\n";
    std::string sequence;
    Machine<std::string, std::string> m("B");
    m.configure("A")
        .onEntry([&sequence](){ std::cout << "entering A\n"; sequence += ">A"; })
        .onExit([&sequence](){ std::cout << "exiting A\n"; sequence += "<A"; });
    m.configure("B")
        .substateOf("A")
        .permit("X", "D")
        .onEntry([&sequence](){ std::cout << "entering B\n"; sequence += ">B"; })
        .onExit([&sequence](){ std::cout << "exiting B\n"; sequence += "<B"; });
    m.configure("C")
        .substateOf("A")
        .onEntry([&sequence](){ std::cout << "entering C\n"; sequence += ">C"; })
        .onExit([&sequence](){ std::cout << "exiting C\n"; sequence += "<C"; });
    m.configure("D")
        .substateOf("C")
        .onEntry([&sequence](){ std::cout << "entering D\n"; sequence += ">D"; })
        .onExit([&sequence](){ std::cout << "exiting D\n"; sequence += "<D"; });
    assert(m.isInState("B"));
    m.fire("X");
    assert(m.isInState("D"));
    std::cout << sequence << "\n";
    assert(sequence == "<B>C>D");
}

void testReentrySubState() {
    /*
            A
            |
            B
    */
    std::cout << "-- testReentrySubState\n";
    std::string sequence;
    Machine<std::string, std::string> m("A");
    m.configure("A")
        .initialTransition("B")
        .permitReentry("X")
        .onEntry([&sequence](){ std::cout << "entering A\n"; sequence += ">A"; })
        .onExit([&sequence](){ std::cout << "exiting A\n"; sequence += "<A"; });
    m.configure("B")
        .substateOf("A")
        .onEntry([&sequence](){ std::cout << "entering B\n"; sequence += ">B"; })
        .onExit([&sequence](){ std::cout << "exiting B\n"; sequence += "<B"; });
    assert(m.isInState("A"));
    m.fire("X");
    assert(m.isInState("B"));
    std::cout << sequence << "\n";
    assert(sequence == "<A>A>B");
}

int main() {
    testPermit();
    testInitialSubState();
    testExitSubState();
    testExitSiblingSubState();
    testExitSuperSubState();
    testEnterSuperSubState();
    testReentrySubState();

    std::cout << "Finished!\n";
}
#include "../machine.h" 

enum class State { Idle, Less, Equal, Greater };
enum class Trigger { Compare, Reset };

State compare(int a, int b) {
    int d = a - b;
    return d < 0 ? State::Less : d > 0 ? State::Greater : State::Equal;
}

int main() {
    Machine<State, Trigger> m(State::Idle);

    m.configure(State::Idle)
        .permitDynamic<int, int>(Trigger::Compare, compare);
    m.configure(State::Less)
        .permit(Trigger::Reset, State::Idle);
    m.configure(State::Equal)
        .permit(Trigger::Reset, State::Idle);
    m.configure(State::Greater)
        .permit(Trigger::Reset, State::Idle);

    assert(m.isInState(State::Idle));
    m.fire(Trigger::Compare, 1, 2);
    assert(m.isInState(State::Less));
    m.fire(Trigger::Reset);
    assert(m.isInState(State::Idle));
}

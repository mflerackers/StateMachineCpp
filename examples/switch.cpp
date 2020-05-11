#ifdef SWITCH_EXAMPLE

#include "../machine.h"

enum class State { Off, On };
enum class Trigger { Switch };

int main() {
    Machine<State, Trigger> m(State::Off);

    m.configure(State::Off).permit(Trigger::Switch, State::On);
    m.configure(State::On).permit(Trigger::Switch, State::Off);

    assert(m.isInState(State::Off));
    m.fire(Trigger::Switch);
    assert(m.isInState(State::On));
}

#endif
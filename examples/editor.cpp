#include "../machine.h" 

enum class State { Play, Edit, Translate, Rotate, Scale };
enum class Trigger { Play, Edit, Translate, Rotate, Scale };

int main() {
    Machine<State, Trigger> m(State::Play);

    m.configure(State::Play)
        .permit(Trigger::Edit, State::Edit);
    m.configure(State::Edit)
        .initialTransition(State::Translate)
        .permit(Trigger::Play, State::Play)
        .permit(Trigger::Translate, State::Translate)
        .permit(Trigger::Rotate, State::Rotate)
        .permit(Trigger::Scale, State::Scale);
    m.configure(State::Translate)
        .substateOf(State::Edit);
    m.configure(State::Rotate)
        .substateOf(State::Edit);
    m.configure(State::Scale)
        .substateOf(State::Edit);
    
    assert(m.isInState(State::Play));
    m.fire(Trigger::Edit);
    assert(m.isInState(State::Edit));
    assert(m.isInState(State::Translate));
    m.fire(Trigger::Play);
    assert(m.isInState(State::Play));
}

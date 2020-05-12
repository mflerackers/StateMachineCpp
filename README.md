[![Run on Repl.it](https://repl.it/badge/github/mflerackers/StateMachineCpp)](https://repl.it/github/mflerackers/StateMachineCpp)

# State Machine

A state machine implementation written in modern C++ based on the API of the stateless C# library.

Working but some features are still untested/unimplemented.

Mostly done for fun as well as due to the lack of a decent (and simple) state machine library.

## Usage

### A simple switch

The following code defines a simple switch which can be turned on and off. In a game, a tap could fire the switch trigger, and the drawing procedure would draw the switch according to which state the switch is in.

Calling permit adds an allowed transition from the configured state state to the given state. The transition happens when the given trigger is fired.

```cpp
enum class State { Off, On };
enum class Trigger { Switch };

Machine<State, Trigger> m(State::Off);

m.configure(State::Off)
    .permit(Trigger::Switch, State::On);
m.configure(State::On)
    .permit(Trigger::Switch, State::Off);

assert(m.isInState(State::Off));
m.fire(Trigger::Switch);
assert(m.isInState(State::On));
```

### Callbacks

Usually in a game, there is some sort of animation between a light being on and off. This can be a flicker when turning on and a fade when turning off for example. To model this we need to know when the state changes, for this we can use callbacks. 

Calling onEntry or onExit adds a callback which will be called when a transition happens.

```cpp
m.configure(State::On)
    .permit(Trigger::Switch, State::Off)
    .onEntry([](){ flicker(); });
    .onExit([](){ fade(); });
```

### Conditions

A switch usually only works when the circuit is powered.

To add this condition we can use permitIf instead of permit. This causes the transition to happen only if the predicate returns true.

Since we are not going to test whether the power is on before firing the trigger, we need to prevent our machine from asserting. The default behaviour when an unhandled trigger is fired asserts. There are two ways to prevent this. One is adding an ignore for the other case.

```cpp
m.configure(State::Off)
    .permitIf(Trigger::Switch, State::On, [](){ return isPowered; })
    .ignoreIf(Trigger::Switch, [](){ return !isPowered; });
```

The other way is to add our own handler for unhandled triggers.

```cpp
m.onUnhandledTrigger([](){});
m.configure(State::Off)
    .permitIf(Trigger::Switch, State::On, [](){ return isPowered; });
```

### Sub states

Sometimes our state diagram is a bit more complicated than that for a switch. Take for example an editor mode inside a game. Once in editing mode, there are tools to translate, rotate or scale objects. But when we are in translate mode, we are also still in edit mode. Exiting edit mode will also exit translate mode.

We can model this with sub states. The substateOf call configures a state to be a substate of another state. As can be seen in the example, when we are in a substate, we are also still in the parent state. So checking whether we are in edit mode is enough to draw the correct UI. To transition back to play mode when in translation mode, the correct permission will be found in the parent state, so sub states don't need explicit transitions to play mode. 

```cpp
enum class State { Play, Edit, Translate, Rotate, Scale };
enum class Trigger { Play, Edit, Translate, Rotate, Scale };

Machine<State, Trigger> m(State::Play);

m.configure(State::Play)
    .permit(Trigger::Edit, State::Edit);
m.configure(State::Edit)
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
m.fire(Trigger::Translate);
assert(m.isInState(State::Edit));
assert(m.isInState(State::Translate));
m.fire(Trigger::Play);
assert(m.isInState(State::Play));
```

### Initial transition

Wouldn't it be nice if we went immediately to translate mode when we entered edit mode? Of course we could make play mode transition to translate mode explicitly by modifying the permit call. However there is another way, we can let edit mode decide which tool is default, which feels cleaner.

This can be done be calling initialTransition and specifying the sub state we want to transition to when entering the edit state.

```cpp
enum class State { Play, Edit, Translate, Rotate, Scale };
enum class Trigger { Play, Edit, Translate, Rotate, Scale };

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
```

### State reentry

### Dynamic destinations

We saw how we could make a transition conditionally by giving a predicate to permitIf. When there are more than 2 options, it can be a hassle to write the correct permitIf statements though.

In these cases permitDynamic can give us a way to write a dynamic transition more naturally.

```cpp
enum class State { Idle, Less, Equal, Greater };
enum class Trigger { Compare, Reset };

Machine<State, Trigger> m(State::Idle);

int compare(int a, int b) [
    int d = a - b;
    return d < 0 ? State::Less : d > 0 ? State::Greater : State::Equal;
]

m.configure(State::Idle)
    .permitDynamic(Trigger::Compare, compare);
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
```
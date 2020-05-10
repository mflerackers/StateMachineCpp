#include <iostream>
#include <cassert>
#include <functional>
#include <map>
#include <memory>

template <typename S, typename T>
class Machine {
public:
    Machine(S initialState) :
    fState(initialState) {

    }

    class MachineState {
    public:
        MachineState(Machine &machine, S state) : 
        fMachine(machine),
        fState(state) {

        }

        // Transition from one state to another state
        MachineState &permit(T trigger, S state) {
            assert(state != fState);
            fTriggers.insert({trigger, std::make_pair(state, nullptr)});
            return *this;
        }

        // Conditional transition from one state to another state
        MachineState &permitIf(T trigger, S state, const std::function<bool()> &predicate) {
            assert(state != fState);
            fTriggers.insert({trigger, std::make_pair(state, predicate)});
            return *this;
        }

        // Transition from one state to the same state
        MachineState &permitReentry(T trigger) {
            fTriggers.insert({trigger, std::make_pair(fState, nullptr)});
            return *this;
        }

        // No transition, but also no handler exception
        MachineState &ignore(T trigger) {
            fTriggers.insert({trigger, std::make_pair(std::nullopt, nullptr)});
            return *this;
        }

        // Conditionally no transition, but also no handler exception
        MachineState &ignoreIf(T trigger, const std::function<bool()> &predicate) {
            fTriggers.insert({trigger, std::make_pair(std::nullopt, predicate)});
            return *this;
        }

        // No transition, but calls action
        MachineState &internalTransition(T trigger, const std::function<void()> &action) {
            fTriggers.insert({trigger, std::make_pair(std::nullopt, nullptr)}); // TODO: call action
            return *this;
        }

        // Conditionally no transition, but also no handler exception
        MachineState &internalTransitionIf(T trigger, const std::function<bool()> &predicate, const std::function<void()> &action) {
            fTriggers.insert({trigger, std::make_pair(std::nullopt, predicate)}); // TODO: call action
            return *this;
        }

        // Makes this state a substate of the given state
        MachineState &substateOf(S state) {
            // TODO: Check for cycles
            fParentState = state;
            return *this;
        }

        // When entering this state, immediatelly go to the given substate
        MachineState &initialTransition(S state) {
            assert(fState != state);
            assert(!fInitialState);
            fInitialState = state;
            return *this;
        }

        // Set a callback for when this state is entered
        MachineState &onEntry(const std::function<void()> &callback) {
            fOnEntry = callback;
            return *this;
        }

        // Set a callback for when this state is exited
        MachineState &onExit(const std::function<void()> &callback) {
            fOnExit = callback;
            return *this;
        }

        using PredicateState = std::pair<std::optional<S>, std::function<bool()>>;

    private:
        friend class Machine;

        PredicateState *getDestinationFor(T trigger) {
            auto range = fTriggers.equal_range(trigger);
            for (auto i = range.first; i != range.second; i++) {
                if (i->second.second) {
                    if (i->second.second()) {
                        return &i->second;
                    }
                }
                else {
                    return &i->second;
                }
            }
            return nullptr;
        }

        bool isDescendantOf(S state) {
            return (fState == state || (fParentState && fMachine.getMachineState(*fParentState)->isDescendantOf(state)));
        }

        bool isAncestorOf(S state) {
            return fMachine.getMachineState(*fParentState)->isDescendantOf(this->fState);
        }

        Machine                                 &fMachine;
        S                                       fState;
        std::optional<S>                        fParentState;
        std::optional<S>                        fInitialState;
        std::multimap<T, PredicateState>        fTriggers; // TODO: use std::variant once std::visit works on iOS
        std::function<void()>                   fOnEntry;
        std::function<void()>                   fOnExit;
    };

    MachineState &configure(S state) {
        return *getCachedMachineState(state);
    }

    bool canFire(T trigger) {
        // Lookup trigger for current state
        auto destination = getDestinationFor(fState, trigger);
        return destination;
    }

    void fire(T trigger) {
        // Lookup current state
        auto source = getMachineState(fState);
        // Lookup trigger getDestinationFor
        auto transition = getDestinationFor(fState, trigger);
        if (!transition) {
            if (fOnUnhandledTrigger) {
                fOnUnhandledTrigger(fState, trigger);
            }
            else {
                throw std::runtime_error("Trigger not handled");
            }
            return;
        }
        // Check if it isn't an ignore
        if (!transition->first) { return; }
        auto destination = getMachineState(*transition->first);
        // Call exit on old state, and get the highest state reachest when exiting
        auto topLevelState = exit(source, destination, source == destination);
        fState = destination->fState;
        // Call entry on new state
        enter(topLevelState, destination, false);
    }

    bool isInState(S state) {
        if (fState == state) {
            return true;
        }
        else {
            auto currentState = getMachineState(fState);
            while (currentState->fParentState) {
                currentState = getMachineState(*currentState->fParentState);
                if (!currentState) { return false; }
                if (currentState->fState == state) {
                    return true;
                }
            }
        }
        return false;
    }

    void onUnhandledTrigger(const std::function<void(S state, T trigger)> &callback) {
        fOnUnhandledTrigger = callback;
    }

    void onTransitioned(const std::function<void(S source, S destination, T trigger)> &callback) {
        fOnTransitioned = callback;
    }

    void describe() {
        std::cout << "Currently in " << fState;
        auto currentState = getMachineState(fState);

        std::cout << ", possible triggers are:\n";
        for (auto &pair : currentState->fTriggers) {
            std::cout << "  " << pair.first << " to state " << (pair.second.first ? *pair.second.first : fState) << "\n";
        }
        while (currentState->fParentState) {
            currentState = getMachineState(*currentState->fParentState);

            for (auto &pair : currentState->fTriggers) {
                std::cout << "  " << pair.first << " to state " << (pair.second.first ? *pair.second.first : fState) << "\n";
            }
        }
    }

private:
    std::shared_ptr<MachineState> getCachedMachineState(S state) {
        auto i = fStates.find(state);
        if (fStates.end() != i) {
            return i->second;
        }
        auto s = std::make_shared<MachineState>(*this, state);
        fStates.insert_or_assign(state, s);
        return s;
    }

    std::shared_ptr<MachineState> getMachineState(S state) {
        auto i = fStates.find(state);
        if (fStates.end() == i) { throw std::runtime_error("State not configured"); }
        return i->second;
    }

    typename MachineState::PredicateState *getDestinationFor(S state, T trigger) {
        auto currentState = getMachineState(state);
        auto destination = currentState->getDestinationFor(trigger);
        // If the trigger is not handled in the current state, check the parent state
        while (!destination && currentState->fParentState) {
            currentState = getMachineState(*currentState->fParentState);
            destination = currentState->getDestinationFor(trigger);
        }
        return destination;
    }

    void enter(const std::shared_ptr<MachineState> &src, const std::shared_ptr<MachineState> &dst, bool initialTransition) {
        if (!initialTransition) {
            // Check if we need to enter the parent state first
            if (dst->fParentState) {
                // Since src has a parent state, it might be that dst is a descendant of src
                /*
                        parent          
                            |
                        src
                            |
                            *
                            |
                        dst
                */
                if (!src->isDescendantOf(*dst->fParentState)) {
                    enter(src, getMachineState(*dst->fParentState), false);
                }
            }
        }
        if (dst->fOnEntry) { 
            dst->fOnEntry(); 
        }
        if (/*src == dst &&*/ dst->fInitialState) {
            fState = *dst->fInitialState;
            auto currentState = getMachineState(fState);
            assert(currentState->fParentState == dst->fState);
            enter(dst, currentState, true);
        }
    }

    std::shared_ptr<MachineState> exit(const std::shared_ptr<MachineState> &src, const std::shared_ptr<MachineState> &dst, bool reentry) {
        // If dst is a descendant of src (or equal to src), there is no reason to exit the state
        if (!reentry && dst->isDescendantOf(src->fState)) {
            /*
                    src
                     |
                     *
                     |
                    dst
            */
            return src;
        }
        if (src->fOnExit) { 
            src->fOnExit();
        }
        // Check if we need to exit the parent state
        if (src->fParentState) {
            // If dst also has a parent state, it might have a common ancestor, which should not be exited
            if (dst->fParentState) {
                // If the destination is not a descendant of the parent of the source, exit the parent and travel up the graph
                if (dst->isDescendantOf(*src->fParentState)) {
                    /*
                            parent   
                               /\    
                              /  *   
                             /    \  
                            src   dst                
                    */
                    return getMachineState(*src->fParentState);
                }
                else {
                    /*
                           ancestor           ancestor  ancestor
                             /  \                 |        |
                         parent  \             parent      |
                           /      \               |        |
                         src      dst            src      dst
                    */
                    return exit(getMachineState(*src->fParentState), dst, false);
                }
            }
            else {
                // The destination is a top state, just exit the parent and travel up the graph
                /*
                       ancestor   dst         
                          |              
                        parent               
                          |                   
                         src                
                */
                return exit(getMachineState(*src->fParentState), dst, false);
            }
        }
        // If we come here, src has no parent
        /*
             src  dst       src   ancestor
                                     |
                                    dst          
        */
        return src;
    }

    void transition(std::shared_ptr<MachineState> &from, std::shared_ptr<MachineState> &to, T trigger) {
        if (fOnTransitioned) {
            fOnTransitioned(from->fState, to->fState, trigger);
        }
    }

    S                                                       fState;
    std::map<S, std::shared_ptr<MachineState>>              fStates;
    std::function<void(S state, T trigger)>                 fOnUnhandledTrigger;
    std::function<void(S source, S destination, T trigger)> fOnTransitioned;
};

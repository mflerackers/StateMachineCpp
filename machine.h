#include <iostream>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <typeindex>

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
        template <typename ...Args>
        MachineState &permit(T trigger, S state) {
            assert(state != fState);
            fTriggers.insert({trigger, std::make_unique<TriggerAction<Args...>>(state)});
            return *this;
        }

        // Conditional transition from one state to another state
        template <typename ...Args>
        MachineState &permitIf(T trigger, S state, const std::function<bool()> &predicate) {
            assert(state != fState);
            fTriggers.insert({trigger, std::make_unique<ConditionalTriggerAction<Args...>>(predicate, state)});
            return *this;
        }

        // Transition from one state to the same state
        template <typename ...Args>
        MachineState &permitReentry(T trigger) {
            fTriggers.insert({trigger, std::make_unique<TriggerAction<Args...>>(fState)});
            return *this;
        }

        // Conditional transition from one state to the same state
        template <typename ...Args>
        MachineState &permitReentryIf(T trigger, const std::function<bool()> &predicate) {
            fTriggers.insert({trigger, std::make_unique<ConditionalTriggerAction<Args...>>(predicate, fState)});
            return *this;
        }

        // Transition from one state to a dynamicly selected state
        template <typename ...Args, typename F>
        MachineState &permitDynamic(T trigger, F selector) {
            fTriggers.insert({trigger, std::make_unique<DynamicTriggerAction<Args...>>(selector)});
            return *this;
        }

        // Conditional transition from one state to a dynamicly selected state
        template <typename ...Args, typename F>
        MachineState &permitDynamicIf(T trigger, F selector, const std::function<bool()> &predicate) {
            fTriggers.insert({trigger, std::make_unique<ConditionalDynamicTriggerAction<Args...>>(predicate, selector)});
            return *this;
        }

        // No transition, but also no handler exception
        template <typename ...Args>
        MachineState &ignore(T trigger) {
            fTriggers.insert({trigger, std::make_unique<TriggerAction<Args...>>()});
            return *this;
        }

        // Conditionally no transition, but also no handler exception
        template <typename ...Args>
        MachineState &ignoreIf(T trigger, const std::function<bool()> &predicate) {
            fTriggers.insert({trigger, std::make_unique<ConditionalTriggerAction<Args...>>(predicate)});
            return *this;
        }

        // No transition, but calls action
        template <typename ...Args>
        MachineState &internalTransition(T trigger, const std::function<void()> &action) {
            fTriggers.insert({trigger, std::make_unique<InternalTriggerAction<Args...>>(action)});
            return *this;
        }

        // Conditionally no transition, but calls action
        template <typename ...Args>
        MachineState &internalTransitionIf(T trigger, const std::function<void()> &action, const std::function<bool()> &predicate) {
            fTriggers.insert({trigger, std::make_unique<ConditionalInternalTriggerAction<Args...>>(predicate, action)});
            return *this;
        }

        // Makes this state a substate of the given state
        MachineState &substateOf(S state) {
            // Check if parent state is not yet set
            assert(!fParentState);
            // Check for cycles
            assert(!isDescendantOf(state));
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

        // Set a callback for when this state is entered
        template<typename ...Args, typename F>
        MachineState &onEntryFrom(T trigger, F callback) {
            fOnEntryWithParameters.template insert_or_assign<Args...>(trigger, std::make_unique<TypedTriggerCallBack<Args...>>(callback));
            return *this;
        }

    private:
        friend class Machine;

        class Action {
        public:
            virtual bool isValid() {
                return true;
            }
        };

        // Decorator to create a conditional version of an action
        template <typename A>
        class Conditional : public A {
        public:
            template <typename ...Args>
            Conditional(const std::function<bool()> &predicate, Args...args) :
            A(args...),
            fPredicate(predicate) {}

            bool isValid() override {
                return fPredicate();
            }

        private:
            std::function<bool()>   fPredicate;
        };

        // An action for permit, permitReentry, ignore
        template <typename ...Args>
        class TriggerAction : public Action {
        public:
            TriggerAction(S state) : 
                fDestination(state) {}
            TriggerAction() : 
                fDestination() {}

            virtual bool hasDestination() {
                return fDestination.operator bool();
            }

            virtual S getDestination(Args...args) {
                return *fDestination;
            }

            virtual void call() {
            }

        private:
            std::optional<S>    fDestination;
        };

        // An action for internalTransition. It doesn't have a state, just an action which is called
        template <typename ...Args>
        class InternalTriggerAction : public TriggerAction<Args...> {
        public:
            InternalTriggerAction(const std::function<void()> &action) :
            TriggerAction<Args...>(),
            fAction(action) {}

            void call() override {
                fAction();
            }

        private:
            std::function<void()>   fAction;
        };

        // An action for permitDynamic. Always has a destination, decided by a selector
        template <typename ...Args>
        class DynamicTriggerAction : public TriggerAction<Args...> {
        public:
            DynamicTriggerAction(const std::function<S(Args...)> &selector) :
            TriggerAction<Args...>(),
            fSelector(selector) {}

            bool hasDestination() override {
                return true;
            }

            S getDestination(Args...args) override {
                return fSelector(args...);
            }

        private:
            std::function<S(Args...)>      fSelector;
        };

        template <typename ...Args> using ConditionalTriggerAction = Conditional<TriggerAction<Args...>>;
        template <typename ...Args> using ConditionalDynamicTriggerAction = Conditional<DynamicTriggerAction<Args...>>;
        template <typename ...Args> using ConditionalInternalTriggerAction = Conditional<InternalTriggerAction<Args...>>;

        template <typename ...Args>
        TriggerAction<Args...> *getActionFor(T trigger) {
            auto range = fTriggers.equal_range(trigger);
            for (auto i = range.first; i != range.second; i++) {
                if (!i->second->isValid()) {
                    continue;
                }
                else {
                    return dynamic_cast<TriggerAction<Args...>*>(i->second.get());
                }
            }
            if (fParentState) {
                return fMachine.getMachineState(*fParentState)->template getActionFor<Args...>(trigger);
            }
            else {
                return nullptr;
            }
        }

        bool isDescendantOf(S state) {
            return (fState == state || (fParentState && fMachine.getMachineState(*fParentState)->isDescendantOf(state)));
        }

        bool isAncestorOf(S state) {
            return fMachine.getMachineState(*fParentState)->isDescendantOf(this->fState);
        }

        template <typename ...Args>
        void callOnEntry(T trigger, Args...args) {
            TriggerCallback *callback = fOnEntryWithParameters.template at<Args...>(trigger);
            if (callback) {
                TypedTriggerCallBack<Args...> *typedCallback = dynamic_cast<TypedTriggerCallBack<Args...>*>(callback);
                assert(typedCallback);
                if (typedCallback) {
                    (*typedCallback)(args...);
                }
            }
            else {
                if (fOnEntry) {
                    fOnEntry();
                }
            }
        }

        template <typename ...Args>
        void callOnExit(T trigger, Args...args) {
            TriggerCallback *callback = fOnExitWithParameters.template at<Args...>(trigger);
            if (callback) {
                TypedTriggerCallBack<Args...> *typedCallback = dynamic_cast<TypedTriggerCallBack<Args...>*>(callback);
                assert(typedCallback);
                if (typedCallback) {
                    (*typedCallback)(args...);
                }
            }
            else {
                if (fOnExit) {
                    fOnExit();
                }
            }
        }

        class TriggerCallback {
        public:
            virtual ~TriggerCallback() {}
        };

        template <typename ...Args>
        class TypedTriggerCallBack : public TriggerCallback {
        public:
            TypedTriggerCallBack(const std::function<void(Args...)> &callback) : fCallback(callback) {}
            void operator()(Args...args) { fCallback(args...); }
        private:
            std::function<void(Args...)> fCallback;
        };

        template <>
        class TypedTriggerCallBack<void> : public TriggerCallback {
        public:
            TypedTriggerCallBack(const std::function<void()> &callback) : fCallback(callback) {}
            void operator()() { fCallback(); }
        private:
            std::function<void()> fCallback;
        };

        struct TriggerMap {
            std::map<std::type_index, TriggerMap>           fSubMap;
            std::map<T, std::unique_ptr<TriggerCallback>>   fCallbacks;

            void insert_or_assign(T trigger, std::unique_ptr<TriggerCallback> &&callback) {
                fCallbacks.insert_or_assign(trigger, std::move(callback));
                //std::cout << "callback for " << trigger << " inserted\n";
            }
            
            template <typename Type>
            void insert_or_assign(T trigger, std::unique_ptr<TriggerCallback> &&callback) {
                //std::cout << "callback for " << trigger << " got submap for " << typeid(Type).name() << "\n";
                fSubMap[std::type_index(typeid(Type))].insert_or_assign(trigger, std::move(callback));
            }

            template <typename First, typename Second, typename ...Rest> // We need second here to distinguish it from a call with one template argument
            void insert_or_assign(T trigger, std::unique_ptr<TriggerCallback> &&callback) {
                fSubMap[std::type_index(typeid(First))].template insert_or_assign<Second, Rest...>(trigger, std::move(callback));
            }

            TriggerCallback *at(T trigger) {
                auto i = fCallbacks.find(trigger);
                //std::cout << "callback for " << trigger << (fCallbacks.end() != i ? " found" : " not found") << "\n";
                return fCallbacks.end() != i ? i->second.get() : nullptr;
            }

            template <typename Type>
            TriggerCallback *at(T trigger) {
                auto i = fSubMap.find(std::type_index(typeid(Type)));
                //std::cout << "callback for " << trigger << " submap for " << typeid(Type).name() << (fSubMap.end() != i ? " found" : " not found") << "\n";
                return fSubMap.end() != i ? i->second.at(trigger) : nullptr;
            }

            template <typename First, typename Second, typename ...Rest>
            TriggerCallback *at(T trigger) {
                auto i = fSubMap.find(std::type_index(typeid(First)));
                return fSubMap.end() != i ? i->second.template at<Second, Rest...>(trigger) : nullptr;
            }
        };

        Machine                                     &fMachine;
        S                                           fState;
        std::optional<S>                            fParentState;
        std::optional<S>                            fInitialState;
        std::multimap<T, std::unique_ptr<Action>>   fTriggers; // TODO: use std::variant once std::visit works on iOS
        TriggerMap                                  fOnEntryWithParameters;
        TriggerMap                                  fOnExitWithParameters;
        std::function<void()>                       fOnEntry;
        std::function<void()>                       fOnExit;
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
        // Lookup trigger action
        auto action = getActionFor(fState, trigger);
        if (!action) {
            if (fOnUnhandledTrigger) {
                fOnUnhandledTrigger(fState, trigger);
            }
            else {
                assert(false);
            }
            return;
        }
        // Check if it isn't an ignore or internal transition
        if (!action->hasDestination()) {
            // If it is an internal transition, call the action
            action->call();
            return; 
        }
        S state = action->getDestination();
        auto destination = getMachineState(state);
        // Call exit on old state, and get the highest state reachest when exiting
        auto topLevelState = exit(source, destination, source == destination);
        fState = destination->fState;
        transitioned(source, destination, trigger);
        // Call entry on new state
        enter(topLevelState, destination, trigger, false);
    }

    template <typename ...Args>
    void fire(T trigger, Args...args) {
        // Lookup current state
        auto source = getMachineState(fState);
        // Lookup trigger action
        auto action = getActionFor<Args...>(fState, trigger);
        if (!action) {
            if (fOnUnhandledTrigger) {
                fOnUnhandledTrigger(fState, trigger);
            }
            else {
                assert(false);
            }
            return;
        }
        // Check if it isn't an ignore or internal transition
        if (!action->hasDestination()) {
            // If it is an internal transition, call the action
            action->call();
            return; 
        }
        auto state = action->getDestination(args...);
        auto destination = getMachineState(state);
        // Call exit on old state, and get the highest state reachest when exiting
        auto topLevelState = exit<Args...>(source, destination, trigger, source == destination, args...);
        fState = destination->fState;
        transitioned(source, destination, trigger);
        // Call entry on new state
        enter<Args...>(topLevelState, destination, trigger, false, args...);
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
    MachineState *getCachedMachineState(S state) {
        auto i = fStates.find(state);
        if (fStates.end() != i) {
            return i->second.get();
        }
        auto pair = fStates.insert_or_assign(state, std::make_unique<MachineState>(*this, state));
        return pair.first->second.get();
    }

    MachineState *getMachineState(S state) {
        assert(fStates.count(state));
        return getCachedMachineState(state);
    }

    template <typename ...Args>
    typename MachineState::template TriggerAction<Args...> *getActionFor(S state, T trigger) {
        auto currentState = getMachineState(state);
        auto destination = currentState->template getActionFor<Args...>(trigger);
        return destination;
    }

    void enter(MachineState *src, MachineState*dst, T trigger, bool initialTransition) {
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
                    enter(src, getMachineState(*dst->fParentState), trigger, false);
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
            enter(dst, currentState, trigger, true);
        }
    }

    template <typename ...Args>
    void enter(MachineState *src, MachineState*dst, T trigger, bool initialTransition, Args...args) {
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
                    enter<Args...>(src, getMachineState(*dst->fParentState), trigger, false, args...);
                }
            }
        }
        dst->template callOnEntry<Args...>(trigger, args...);
        if (/*src == dst &&*/ dst->fInitialState) {
            fState = *dst->fInitialState;
            auto currentState = getMachineState(fState);
            assert(currentState->fParentState == dst->fState);
            enter<Args...>(dst, currentState, trigger, true, args...);
        }
    }

    MachineState *exit(MachineState *src, MachineState *dst, bool reentry) {
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

    template <typename ...Args>
    MachineState *exit(MachineState *src, MachineState *dst, T trigger, bool reentry, Args...args) {
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
        src->template callOnExit<Args...>(trigger, args...);
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
                    return exit<Args...>(getMachineState(*src->fParentState), dst, trigger, false, args...);
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
                return exit<Args...>(getMachineState(*src->fParentState), dst, trigger, false, args...);
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

    void transitioned(MachineState *from, MachineState *to, T trigger) {
        if (fOnTransitioned) {
            fOnTransitioned(from->fState, to->fState, trigger);
        }
    }

    S                                                       fState;
    std::map<S, std::unique_ptr<MachineState>>              fStates;
    std::function<void(S state, T trigger)>                 fOnUnhandledTrigger;
    std::function<void(S source, S destination, T trigger)> fOnTransitioned;
};

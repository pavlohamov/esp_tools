/*
 * FineStateMachine.h
 *
 *  Created on: April 13, 2020
 *      Author: Pavlo Hamov
 */

#pragma once


#include <stdint.h>

namespace SM {


#ifndef FSM_MAX_DEPTH
#define FSM_MAX_DEPTH 16
#endif

class FSM;

typedef uint32_t FSM_Signal_t;

enum {
    FSM_Signal_PARENT, // used to get parent state
    FSM_Signal_ENTRY, // once state is being entred
    FSM_Signal_STABLE, // once tran is completed and state is stable
    FSM_Signal_EXIT, // once exits from state

    FSM_Signal_routine, // for superloops

    FSM_Signal_User
};

class FSM_Event {
public:
    constexpr FSM_Event(FSM_Signal_t signal): sig(signal) {};
    constexpr virtual ~FSM_Event() {};

    FSM_Signal_t sig;
};


/**
 * @return FSM_State_t
 */

typedef void* (*FSM_State_t) (FSM *const fsm, const FSM_Event *event);

class FSM {
public:
    FSM(): state(Default), target(Default) {}
	virtual ~FSM() {};

    /**
     * @brief prepare for using
     *
     * @param[in]  fsm      the state machine object
     * @param[in]  state    Initial state. It's parent MUST be a FSM_Default
     */
    static void Init(FSM *const fsm, const FSM_State_t state);
    void Init(const FSM_State_t state) { Init(this, state); }

    /**
     * @brief make transition to given state
     *
     * @param[in]  fsm      the state machine object
     * @param[in]  state    State where to. In case of derived (hierarchy) structure,
     *                      exit will be performed to nearest parent for bot state (current & target)
     */
    static void Tran(FSM *const fsm, const FSM_State_t state);
    void Tran(const FSM_State_t state) { Tran(this, state); }

    /**
     * @brief Process event by given SM
     *
     * @param[in]  fsm      the state machine object
     * @param[in]  event    event for processing
     */
    static void Dispatch(FSM *const fsm, const FSM_Event *event);
    void Dispatch(const FSM_Event *event) { Dispatch(this, event); }

    /**
     * @brief   Get current state. Changed in end of transition.
     *          Could be used in state Entry to determine previous state
     * @param[in]  fsm      the state machine object
     * @return current state
     */
    static FSM_State_t getState(const FSM *const fsm) { return fsm->getState(); }
    FSM_State_t getState() const { return state; }

    /**
     * @brief Get target state. Changed in start of transition
     * @param[in]  fsm      the state machine object
     * @return target state
     */
    static FSM_State_t getTargetState(const FSM *const fsm) { return fsm->getTargetState(); }
    FSM_State_t getTargetState() const { return target; }

    /**
     * @brief Check if fsm is in derived state.
     * @param[in]  fsm      the state machine object
     * @param[in]  state    state to check in hierarchy
     * @return true if in hierarchy
     */
    static bool isInState(const FSM *const fsm, const FSM_State_t state) { return fsm->getTargetState(); }
    bool isInState(const FSM_State_t state) const;

protected:
    /**
     * @brief  default state for all SM's
     */
    static void* Default(FSM *const fsm, const FSM_Event *event);

    /**
     * @brief  use to indicate that event is consumed
     */
    static void* Handled(FSM *const fsm, const FSM_Event *event);

private:
    FSM_State_t state;
    FSM_State_t target;

private:
    static uint32_t getParents(FSM *const fsm, const FSM_State_t state, FSM_State_t *result);
};

};

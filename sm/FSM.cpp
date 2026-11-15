/*
 * FSM.c
 *
 *  Created on: April 13, 2020
 *      Author: Pavlo Hamov
 */

#include "FSM.hpp"

#include <stddef.h>

using namespace SM;

static const FSM_Event s_evtGetParent(FSM_Signal_PARENT);
static const FSM_Event s_evtEntry(FSM_Signal_ENTRY);
static const FSM_Event s_evtStable(FSM_Signal_STABLE);
static const FSM_Event s_evtExit(FSM_Signal_EXIT);

void FSM::Init(FSM *const fsm, const FSM_State_t state) {
//    // no derived hierarchy at start!
//    if ((state == Default) || (state(fsm, &s_evtGetParent) != Default))
//        return;
    fsm->state = state;
    fsm->target = state;

    FSM_State_t states[FSM_MAX_DEPTH];
    const uint32_t stateCnt = getParents(fsm, fsm->state, states);

    for (int32_t i = stateCnt; i >= 0; i--) {
        if (states[i])
            states[i](fsm, &s_evtEntry);
    }

    fsm->target(fsm, &s_evtStable);
}

void FSM::Tran(FSM *const fsm, const FSM_State_t state) {
    // cannot change state during state change
    if (fsm->state != fsm->target)
        return;
    fsm->target = state;
    FSM_State_t states[FSM_MAX_DEPTH];
    FSM_State_t targets[FSM_MAX_DEPTH];

    const uint32_t stateCnt = getParents(fsm, fsm->state, states);
    const uint32_t targetCnt = getParents(fsm, fsm->target, targets);
    int32_t sCnt = stateCnt;
    int32_t tCnt = targetCnt;

    while ((sCnt >= 0) && (tCnt >= 0)) {
        if (states[sCnt] != targets[tCnt])
            break;
        states[sCnt--] = NULL;
        targets[tCnt--] = NULL;
    }
    // exit from all
    for (uint32_t i = 0; i < stateCnt; i++) {
        if (states[i])
            states[i](fsm, &s_evtExit);
    }
    // entry for all
    for (int32_t i = targetCnt; i >= 0; i--) {
        if (targets[i])
            targets[i](fsm, &s_evtEntry);
    }
    fsm->state = fsm->target;
    // huh. Finally in target state
    fsm->target(fsm, &s_evtStable);
}

void FSM::Dispatch(FSM *const fsm, const FSM_Event *event) {
    // cannot handle event during transition
    if (fsm->state != fsm->target)
        return;

    FSM_State_t state = fsm->state;
    do {
        state = reinterpret_cast<FSM_State_t>(state(fsm, event));
    } while (state != reinterpret_cast<FSM_State_t>(Handled));
}

bool FSM::isInState(const FSM_State_t state) const {
    // cannot handle event during transition
    if (this->state != target)
        return false;
    FSM_State_t sts = this->state;
    while (sts && (sts != Default)) {
        if (sts == state)
            return true;
        sts = reinterpret_cast<FSM_State_t>(sts(const_cast<FSM*>(this), &s_evtGetParent));
    }
    return false;
}

uint32_t FSM::getParents(FSM *const fsm, const FSM_State_t state, FSM_State_t *result) {
    result[0] = state;
    uint32_t cnt;
    for (cnt = 1; cnt < FSM_MAX_DEPTH; cnt++) {
        result[cnt] = reinterpret_cast<FSM_State_t>(result[cnt - 1](fsm, &s_evtGetParent));
        if (result[cnt] == reinterpret_cast<FSM_State_t>(Default) || !result[cnt])
            break;
    }
    return cnt;
}

void* FSM::Default(FSM *const fsm, const FSM_Event *event) {
    (void)fsm;
    (void)event;
    // black hole for all events
    return reinterpret_cast<void*>(Handled);
}

void* FSM::Handled(FSM *const fsm, const FSM_Event *event) {
    (void)fsm;
    (void)event;
    return NULL;
}

#include "button_actions.h"
using base::action_on_release;
using base::ButtonAction;
static_assert(action_on_release(0) == ButtonAction::TestContact);
static_assert(action_on_release(999) == ButtonAction::TestContact);
static_assert(action_on_release(1000) == ButtonAction::Wake);
static_assert(action_on_release(2999) == ButtonAction::Wake);
static_assert(action_on_release(3000) == ButtonAction::Commission);
static_assert(action_on_release(9999) == ButtonAction::Commission);
static_assert(action_on_release(10000) == ButtonAction::FactoryReset);
static_assert(action_on_release(60000) == ButtonAction::FactoryReset);

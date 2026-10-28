/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include "fprintd-source/src/goodix-prompt.h"
int main (void)
{
  GoodixPrompt p = {1,0,0};
  assert (!goodix_prompt_take (&p));
  p.replied=1;
  assert (!goodix_prompt_take (&p));
  p.ready=1;
  assert (goodix_prompt_take (&p));
  assert (!goodix_prompt_take (&p));
  p=(GoodixPrompt){1,1,0};
  assert (!goodix_prompt_take (&p));
  p.replied=1;
  assert (goodix_prompt_take (&p));
  p=(GoodixPrompt){1,0,1};
  p.pending=0; /* cancellation or failed preparation */
  p.ready=1;
  assert (!goodix_prompt_take (&p));
  p=(GoodixPrompt){1,0,1}; /* next action cannot reuse ready */
  assert (!goodix_prompt_take (&p));
  return 0;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Only schedules an informational prompt. Never decides authentication. */
typedef struct {
  int pending;
  int ready;
  int replied;
} GoodixPrompt;

static inline int goodix_prompt_take (GoodixPrompt *p)
{
  if (!p->pending || !p->ready || !p->replied)
    return 0;
  p->pending = 0;
  return 1;
}

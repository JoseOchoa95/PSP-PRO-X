# PRO-X Memory Stick Change Probe

MSCHANGE checks whether PSP 6.61 reports a change of medium independently of card contents. It registers for Sony Memory Stick insertion and ejection callbacks and samples slot presence. The diagnostic keeps its observations in RAM and issues no sector reads or writes. A new callback, an empty slot or an error invalidates a marked review. The invalidation remains set when a card returns until the user marks a fresh review.

1. Copy the `PSP` folder inside `release/` to the root of a working launch card, then open **PRO-X Memory Stick Change Probe**. Wait for the Memory Stick light to go out before each removal or reinsertion. `Callback registration: 0x00000000` indicates the monitor started. There may be an initial insertion notification for the card already present.
2. Press X and record the screen with `Review: CURRENT`. Remove the card and adapter, wait for the empty-slot screen, and reinsert the same combination. Record the screen again before pressing X. It should report `Slot: present`, `Review: INVALIDATED` and one or more callback batches since the mark. Both insertion and ejection counters should increase when each event is delivered separately.
3. With the app still running, insert a card that the PSP detects but already asks to format or cannot open, if available. Press X once the presence and callback counters settle. Repeat the removal and reinsertion without pressing X and record the final screen. The PSP may report a FAT error through the callback. The presence fallback can also invalidate the review if a callback is missing, so report the counters and final state together.
4. Press Circle to unregister the callback and return to the PSP menu.

This diagnostic observes the Memory Stick slot during an active run. On the owner's PSP, a card that triggered a system format prompt produced two callback batches after the mark. The last callback carried `0x8001B006`, and the mark remained invalidated. A separate capacity and address-independence check is required before formatting blank media.

Thanks to [PSPDEV contributors](https://github.com/pspdev/pspsdk/blob/master/src/user/pspmscm.h) for documenting the callback and presence APIs.

# Style guide

This project aims for a minimal C implementation of Dangerous Dave in the Haunted Mansion. For convenience, we are compiling using C++, but are mostly using C features.

Dependencies should be kept minimal, ideally just whatever platform SDKs provide. We may opt for SDL2 or other small libraries where it makes sense.

In terms of the implementation accuracy, conceptually we're going for "95%" — that is, there are a lot of details in the original binaries necessary for a DOS game written in 1991. Many of those limitations no longer apply, but in most cases we should defer to the original look and feel of the game where possible.

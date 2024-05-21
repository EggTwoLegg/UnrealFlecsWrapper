# UECS | An Unreal Engine 5 + Flecs integration
#### _*Meant for C++ developers. No blueprint support (yet and potentially ever)._

UECS is a thin Flecs and Unreal Engine integration module that I use to support my upcoming titles.
It is made with iteration performance in mind and carries certain tradeoffs, such as increased boilerplate
requirements and explicit FTask creation. It is not a plug-and-play solution, nor will it ever be.
It is, however, able to seamlessly integrate with the Task Graph system and iterate concurrently and safely
among as many worker threads as your target platform will support.

## Attributions
Flecs (https://github.com/SanderMertens/flecs)
ConcurrentQueue (https://github.com/cameron314/concurrentqueue)

## Installation
Move the UECS module folder into your project's source. Follow standard procedure to include its public
and private dependencies.

## Development
 (TO BE DONE)

## License

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of
conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or other materials
provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# 🌍 ArchipelaWoW

This is a module for [AzerothCore](http://www.azerothcore.org) that adds support for [Archipelago](https://github.com/ArchipelagoMW/Archipelago) - a multi-game multi-world randomizer framework.

This repository contains the code for the [client](https://archipelago.miraheze.org/wiki/Client), you can find the [APWorld](https://archipelago.miraheze.org/wiki/APWorld) repository [here](https://github.com/r-o-b-o-t-o/archipelawow).

> ❔ What's with the weird module name?  
> It was originally named `mod-archipelawow`. However, I really wanted to integrate with `mod-autobalance` which brings experience scaling in dungeons. This module's XP gain hook conflicted with the one from AutoBalance, the only way to fix it was a rename (module hooks are fired in order of the module name). I wasn't able to come up with an interesting replacement name for the project as a whole that would come after `autobalance` so I decided to only rename the module with something that's reminiscent of multiplayer Archipelago games. Naming things is one of the hardest parts of software engineering, after all.

## 🚀 Installation

### Prerequisites

> [!WARNING]
> The changes from [#25295](https://github.com/azerothcore/azerothcore-wotlk/pull/25295) must be applied to your AzerothCore local clone.

The following modules are required for full progression:
- [DungeonRespawn](https://github.com/Dreathean/DungeonRespawn)
- [mod-autobalance](https://github.com/azerothcore/mod-autobalance)
- [mod-solo-lfg](https://github.com/azerothcore/mod-solo-lfg)

### Setup guide

1. **Clone the repository** into the `modules` folder of your AzerothCore local clone
   ```bash
   cd path/to/azerothcore-wotlk/modules
   git clone https://github.com/r-o-b-o-t-o/mod-i-found-your-sword.git
   ```

2. **Re-run CMake**

3. **Build AzerothCore**

4. **Copy the configuration file**
   - Locate the configuration directory of your AzerothCore installation, usually `configs` for Windows or `etc` for Linux
   - In the `modules` subdirectory, copy `archipelawow.conf.dist` into `archipelawow.conf`

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

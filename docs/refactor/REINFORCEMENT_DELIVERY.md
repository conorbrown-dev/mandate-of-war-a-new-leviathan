# Reinforcement Delivery

```bash
RTS_REINFORCEMENT_DELIVERY_SMOKE=1 ./Godot_v4.7.2-stable_linux.x86_64 --path godot/project
```

Press `D`, left-click the cyan friendly zone, then press `F`. Native simulation deducts 260 Materials and 140 Energy once, advances a four-second delivery, and creates one Industrial MBT at completion. Invalid requests do not deduct resources.

Automated evidence:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tools/validate.py reinforcement_delivery_smoke --rendered --require-screenshots --timeout 120
```

The delivery configuration lives in `src/simulation/reinforcement_delivery.hpp`:
one Industrial MBT, 260 Materials, 140 Energy, and 4000 ms delivery duration.
The scenario starts with 600 Materials and 400 Energy in `main.gd` so the cost
change is inspectable. This is one owned native zone only; no supply network,
multiple packages, transport combat, or strategic-map routing is included.

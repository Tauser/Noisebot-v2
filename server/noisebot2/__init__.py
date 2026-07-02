"""NoiseBot 2 — a mente.

Cinco atores sobre um event bus interno (ver docs/ARCHITECTURE.md §7):
TurnEngine, MindOutput, VisionMind, PersonaMind, SkillHost.
Regra estrutural: nenhum ator chama outro — comunicação apenas via bus.

Esqueleto S1: os módulos declaram contratos; implementação entra por fase
(S4: turn_engine/mind_output; S5: vision_mind; S7: persona_mind/skill_host).
"""

__version__ = "0.1.0"

"""Smoke do esqueleto — mantém o job server-tests vivo desde o commit 1.

Substituído por testes reais dos atores a partir de S4.4. A regra que este
arquivo lembra: o CI nunca fica sem testes para rodar, e nunca é filtrado por
paths (docs/QUALITY.md §1).
"""

import noisebot2


def test_package_imports():
    assert noisebot2.__version__

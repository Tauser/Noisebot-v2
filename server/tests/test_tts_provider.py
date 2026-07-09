from __future__ import annotations

from pathlib import Path

import pytest

from noisebot2.providers import PiperTtsProvider, TtsProviderError


@pytest.mark.asyncio
async def test_piper_provider_fails_explicitly_when_model_is_missing(tmp_path: Path) -> None:
    provider = PiperTtsProvider(model=str(tmp_path / "missing.onnx"))

    with pytest.raises(TtsProviderError, match="modelo Piper nao encontrado"):
        async for _chunk in provider.synthesize("ola"):
            pass

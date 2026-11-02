# Contributing

Target the original Ultimate 64 / Elite Nios-II branch and preserve the pinned upstream provenance. Keep unrelated FPGA experiments and other board ports separate.

For changes, describe the concrete problem, resulting behavior, source of any imported code, and validation performed. Preserve copyright/license notices. Do not commit passwords, private network configuration, vendor installers or generated build products.

Run focused native tests for the changed behavior, then build with Quartus/Nios-II 18.1 and ESP-IDF v5.3.1 where relevant. Record hardware testing separately from host tests. Before distributing a firmware binary, record its source commit, tool versions, SHA-256, board/core/bridge versions and remaining limitations.

Use GitHub issues for reproducible defects. Include reproduction steps and sanitized logs. Never treat a successfully linked updater as proof that all boards or runtime paths have passed.

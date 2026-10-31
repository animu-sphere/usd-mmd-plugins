#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fail if a selected job did not succeed, or an unselected job did not skip."""

import json
import os


def check_gate(needs: dict) -> None:
    if needs["changes"]["result"] != "success" or needs["docs"]["result"] != "success":
        raise ValueError("Change classification and cheap checks must succeed")
    plan = needs["changes"]["outputs"]
    for job in ("source", "parser", "physics"):
        selected = plan.get(f"run_{job}")
        if selected not in {"true", "false"}:
            raise ValueError(f"Missing or invalid selection for {job}")
        expected = "success" if selected == "true" else "skipped"
        if needs[job]["result"] != expected:
            raise ValueError(f"{job}: expected {expected}, got {needs[job]['result']}")


if __name__ == "__main__":
    check_gate(json.loads(os.environ["CI_NEEDS"]))
    print("All selected CI contracts passed.")

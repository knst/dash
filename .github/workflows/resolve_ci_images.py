# Copyright (c) 2026 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Find trusted CI container images that this run can reuse instead of rebuilding.

Runs in build.yml's check-skip job, which only checks out base branch code,
even under pull_request_target. The commit under test is read only as data
through the GitHub API; nothing from it is checked out or executed here.

The reuse key covers everything that decides what gets built: the git tree id
of contrib/containers/ci at the commit under test, the ubuntu:noble manifest
digest, and the workflows in this checkout that drive the build (the same
revision GitHub runs). Only trusted (push) builds tag images with that key, so
a hit is an image that a trusted run built the same way from the same context.
Any failure means "build as usual", never a failed run.
"""

import argparse
import base64
import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from typing import Callable, Dict, Optional, Sequence, Tuple


CONTEXT_DIR = "contrib/containers/ci"
# Workflows that decide how the images are built from CONTEXT_DIR
RECIPE_FILES = (
    ".github/workflows/build.yml",
    ".github/workflows/build-container.yml",
)
TAG_PREFIX = "ctx"
REGISTRY = "ghcr.io"
# (output key, image name) for every image built from CONTEXT_DIR
IMAGES = (
    ("runner", "dashcore-ci-runner"),
    ("slim", "dashcore-ci-slim"),
)
INDEX_MEDIA_TYPES = (
    "application/vnd.oci.image.index.v1+json",
    "application/vnd.docker.distribution.manifest.list.v2+json",
)
REQUIRED_PLATFORMS = frozenset({("linux", "amd64"), ("linux", "arm64")})
REQUEST_TIMEOUT_SECONDS = 10
# Commit and tree ids, SHA-1 or SHA-256 repositories
OBJECT_ID_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
DIGEST_RE = re.compile(r"^[0-9a-f]{64}$")


class HttpError(Exception):
    def __init__(self, status: int, url: str):
        super().__init__("HTTP {} for {}".format(status, url))
        self.status = status


# (url, request headers) -> (lowercased response headers, body)
HttpGet = Callable[[str, Dict[str, str]], Tuple[Dict[str, str], bytes]]


def http_get(url: str, headers: Dict[str, str]) -> Tuple[Dict[str, str], bytes]:
    request = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=REQUEST_TIMEOUT_SECONDS) as response:
            return {k.lower(): v for k, v in response.headers.items()}, response.read()
    except urllib.error.HTTPError as exc:
        raise HttpError(exc.code, url) from exc


def recipe_digest(paths: Sequence[str]) -> str:
    """Hash of the local files that drive the build."""
    digest = hashlib.sha256()
    for path in paths:
        with open(path, "rb") as fh:
            content = fh.read()
        digest.update("{}\0{}\0".format(path, len(content)).encode())
        digest.update(content)
    return digest.hexdigest()


def commit_under_test(event_name: str, event: Dict, github_sha: str) -> str:
    """The commit whose container context the build jobs would use."""
    if event_name == "pull_request_target":
        # build-container.yml checks out the pull request head, not github.sha
        # (which is the base branch tip for pull_request_target).
        return ((event.get("pull_request") or {}).get("head") or {}).get("sha") or ""
    return github_sha


def context_tree_id(get: HttpGet, api_url: str, repo: str, commit: str, token: str) -> str:
    """Tree id of CONTEXT_DIR at commit, read from the API without a checkout."""
    if not OBJECT_ID_RE.match(commit):
        raise ValueError("invalid commit id {!r}".format(commit))
    parent, name = CONTEXT_DIR.rsplit("/", 1)
    url = "{}/repos/{}/contents/{}?ref={}".format(api_url, repo, parent, commit)
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "dash-ci-image-resolver",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    if token:
        headers["Authorization"] = "Bearer {}".format(token)
    _, body = get(url, headers)
    for entry in json.loads(body):
        if entry.get("name") == name and entry.get("type") == "dir":
            tree = entry.get("sha") or ""
            if not OBJECT_ID_RE.match(tree):
                raise ValueError("invalid tree id {!r}".format(tree))
            return tree
    raise LookupError("{} not found at {}".format(CONTEXT_DIR, commit))


def registry_token(get: HttpGet, image_path: str, token: str, actor: str) -> str:
    """Pull token for image_path, authenticated if possible, else anonymous."""
    url = "https://{}/token?{}".format(REGISTRY, urllib.parse.urlencode({
        "scope": "repository:{}:pull".format(image_path),
        "service": REGISTRY,
    }))
    attempts = []
    if token:
        credentials = "{}:{}".format(actor or "github-actions", token).encode()
        attempts.append({"Authorization": "Basic " + base64.b64encode(credentials).decode()})
    attempts.append({})

    last_error: Exception = LookupError("no registry token for {}".format(image_path))
    for headers in attempts:
        try:
            _, body = get(url, headers)
            value = json.loads(body).get("token")
            if value:
                return value
        except Exception as exc:  # noqa: BLE001
            last_error = exc
    raise last_error


def multi_arch_digest(get: HttpGet, image_path: str, tag: str, token: str) -> str:
    """Digest of the multi-arch index tagged `tag`, after checking it is usable."""
    url = "https://{}/v2/{}/manifests/{}".format(REGISTRY, image_path, tag)
    headers = {
        "Accept": ", ".join(INDEX_MEDIA_TYPES),
        "Authorization": "Bearer {}".format(token),
    }
    response_headers, body = get(url, headers)

    media_type = response_headers.get("content-type", "").split(";")[0].strip()
    if media_type not in INDEX_MEDIA_TYPES:
        raise ValueError("{} is {}, not a multi-arch index".format(tag, media_type or "untyped"))

    # Hash what we received rather than trusting the header, so the reference
    # handed to the build jobs names exactly the index that was checked.
    digest = "sha256:" + hashlib.sha256(body).hexdigest()
    header_digest = response_headers.get("docker-content-digest", "")
    if header_digest and header_digest != digest:
        raise ValueError("{} digest mismatch: header {} body {}".format(tag, header_digest, digest))

    platforms = set()
    for manifest in json.loads(body).get("manifests", []):
        platform = manifest.get("platform") or {}
        platforms.add((str(platform.get("os")), str(platform.get("architecture"))))
    missing = REQUIRED_PLATFORMS - platforms
    if missing:
        raise ValueError("{} lacks {}".format(
            tag, ", ".join(sorted("/".join(p) for p in missing))))
    return digest


def resolve_images(
    event_name: str,
    event: Dict,
    github_sha: str,
    repo: str,
    base_digest: str,
    recipe: str,
    get: HttpGet,
    api_url: str = "https://api.github.com",
    token: str = "",
    actor: str = "",
    disabled: bool = False,
) -> Dict[str, str]:
    outputs = {"context_tag": "", "commit": "", "tree": ""}
    for key, _ in IMAGES:
        outputs["{}_image".format(key)] = ""
        outputs["{}_reason".format(key)] = ""

    def build_all(reason: str) -> Dict[str, str]:
        for key, _ in IMAGES:
            outputs["{}_reason".format(key)] = reason
        return outputs

    if not DIGEST_RE.match(base_digest):
        return build_all("build: ubuntu:noble digest unknown ({!r})".format(base_digest))
    if not DIGEST_RE.match(recipe):
        return build_all("build: cannot hash the build workflows")

    commit = commit_under_test(event_name, event, github_sha)
    outputs["commit"] = commit
    try:
        tree = context_tree_id(get, api_url, repo, commit, token)
    except Exception as exc:  # noqa: BLE001
        return build_all("build: cannot read {} tree ({}: {})".format(
            CONTEXT_DIR, type(exc).__name__, exc))
    outputs["tree"] = tree

    tag = "{}-{}-{}-{}".format(TAG_PREFIX, tree, base_digest[:12], recipe[:12])
    # Handed to the container builds even when nothing is reused: trusted
    # builds publish it so later runs can find the image.
    outputs["context_tag"] = tag

    if disabled:
        return build_all("build: reuse disabled by SKIP_CI_IMAGE_REUSE")

    for key, name in IMAGES:
        image_path = "{}/{}".format(repo.lower(), name)
        try:
            registry = registry_token(get, image_path, token, actor)
            digest = multi_arch_digest(get, image_path, tag, registry)
        except Exception as exc:  # noqa: BLE001
            if isinstance(exc, HttpError) and exc.status == 404:
                reason = "build: no trusted image tagged {}".format(tag)
            else:
                reason = "build: registry lookup failed ({}: {})".format(type(exc).__name__, exc)
            outputs["{}_reason".format(key)] = reason
        else:
            outputs["{}_image".format(key)] = "{}/{}@{}".format(REGISTRY, image_path, digest)
            outputs["{}_reason".format(key)] = "reuse: trusted image tagged {}".format(tag)

    return outputs


def write_github_output(path: Optional[str], outputs: Dict[str, str]) -> None:
    if not path:
        return
    with open(path, "a", encoding="utf-8") as fh:
        for key in ["context_tag"] + ["{}_image".format(key) for key, _ in IMAGES]:
            fh.write("{}={}\n".format(key, outputs[key]))


def summary_lines(outputs: Dict[str, str], base_digest: str, recipe: str) -> Sequence[str]:
    lines = [
        "### CI container images",
        "- Context: `{}` tree `{}` at `{}`, ubuntu:noble `{}`, build workflows `{}`".format(
            CONTEXT_DIR, outputs["tree"][:12] or "?", outputs["commit"][:12] or "?",
            base_digest[:12] or "?", recipe[:12] or "?"),
        "- Context tag: `{}`".format(outputs["context_tag"] or "none"),
    ]
    for key, name in IMAGES:
        line = "- {}: {}".format(name, outputs["{}_reason".format(key)])
        path = outputs["{}_image".format(key)]
        if path:
            line += " `{}`".format(path)
        lines.append(line)
    return lines


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Find reusable trusted CI container images.")
    parser.add_argument("--event-name", default=os.environ.get("GITHUB_EVENT_NAME", ""))
    parser.add_argument("--event-path", default=os.environ.get("GITHUB_EVENT_PATH", ""))
    parser.add_argument("--sha", default=os.environ.get("GITHUB_SHA", ""))
    parser.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", ""))
    parser.add_argument("--api-url", default=os.environ.get("GITHUB_API_URL", "https://api.github.com"))
    parser.add_argument("--actor", default=os.environ.get("GITHUB_ACTOR", ""))
    parser.add_argument("--token", default=os.environ.get("GH_TOKEN", ""))
    parser.add_argument(
        "--base-image-digest",
        default=os.environ.get("BASE_IMAGE_DIGEST", ""),
        help="ubuntu:noble manifest digest without the sha256: prefix",
    )
    parser.add_argument(
        "--disable",
        action="store_true",
        default=os.environ.get("SKIP_CI_IMAGE_REUSE", "") != "",
        help="Always build; still report the context tag for trusted builds to publish",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    event: Dict = {}
    if args.event_path:
        try:
            with open(args.event_path, "r", encoding="utf-8") as fh:
                event = json.load(fh)
        except (OSError, ValueError) as exc:
            print("warning: cannot read event payload: {}".format(exc), file=sys.stderr)
        if not isinstance(event, dict):
            event = {}
    try:
        recipe = recipe_digest(RECIPE_FILES)
    except OSError as exc:
        print("warning: cannot hash build workflows: {}".format(exc), file=sys.stderr)
        recipe = ""

    outputs = resolve_images(
        event_name=args.event_name,
        event=event,
        github_sha=args.sha,
        repo=args.repo,
        base_digest=args.base_image_digest,
        recipe=recipe,
        get=http_get,
        api_url=args.api_url,
        token=args.token,
        actor=args.actor,
        disabled=args.disable,
    )

    lines = summary_lines(outputs, args.base_image_digest, recipe)
    print("\n".join(lines))
    write_github_output(os.environ.get("GITHUB_OUTPUT"), outputs)
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as fh:
            fh.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

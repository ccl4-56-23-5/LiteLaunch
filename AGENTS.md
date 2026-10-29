# Release authorization

On 2026-10-29 the user authorized direct local installation to the existing target followed by publication of future updates to this existing GitHub repository after compilation and necessary automated validation, without waiting for personal testing or another confirmation. Publish source, tags and release assets, then verify actual downloaded source and executable hashes. Record automated checks and human acceptance separately.

Preserve user data and existing licenses. Exclude credentials, user settings, private paths, runtime logs and build caches. Verify downloaded source and release hashes before recycling older ordinary local versions; preserve registered baselines.

After verified publication, retain only the latest local source and current deliverables for each component. Keep protected baselines and uncommitted work. A standalone plugin release must use make_latest=false so that the host updater continues to receive its Setup release.

#!/bin/bash
# Credits: https://gist.github.com/domenic/ec8b0fc8ab45f39403dd

set -ex

SOURCE_BRANCH="master"
TARGET_BRANCH="gh-pages"
DEPLOY_TAG="deploy"

function update_readme {
  sed -i "/## Performance/q" docs/README.md
  local BUILD_URL="https://travis-ci.org/$TRAVIS_REPO_SLUG/builds/$TRAVIS_BUILD_ID"
  echo "Tested using CRoaring's \`census1881\` dataset on [travis]($BUILD_URL):" >> docs/README.md
  echo "" >> docs/README.md
  cat "/tmp/test.out" >> docs/README.md
}

# Pull requests and commits to other branches shouldn't try to deploy, just build to verify
if [ "$TRAVIS_PULL_REQUEST" != "false" -o "$TRAVIS_BRANCH" != "$SOURCE_BRANCH" -o "$TRAVIS_TAG" == "$DEPLOY_TAG" ]; then
    echo "Skipping deploy."
    exit 0
fi

# Save some useful information
REPO=`git config remote.origin.url`
SSH_REPO=${REPO/https:\/\/github.com\//git@github.com:}
SHA=`git rev-parse --verify HEAD`

# Now let's go have some fun with the cloned repo
git config user.name "Travis CI"
git config user.email "$COMMIT_AUTHOR_EMAIL"

# Commit the "changes", i.e. the new version.
# The delta will show diffs between new and old versions.
update_readme
git add -A .
git commit -m "Deploy to GitHub Pages: ${SHA}"
git tag -f -a "$DEPLOY_TAG" -m "$DEPLOY_TAG"

# Get the deploy key by using Travis's stored variables to decrypt deploy_key.enc
ENCRYPTED_KEY_VAR="encrypted_${ENCRYPTION_LABEL}_key"
ENCRYPTED_IV_VAR="encrypted_${ENCRYPTION_LABEL}_iv"
ENCRYPTED_KEY=${!ENCRYPTED_KEY_VAR}
ENCRYPTED_IV=${!ENCRYPTED_IV_VAR}
openssl aes-256-cbc -K $ENCRYPTED_KEY -iv $ENCRYPTED_IV -in deploy_key.enc -out deploy_key -d
chmod 600 deploy_key
eval `ssh-agent -s`
ssh-add deploy_key

# Now that we're all set up, we can push.
git push $SSH_REPO $TARGET_BRANCH

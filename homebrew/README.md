# Homebrew Tap for ss-apple

This directory contains the Homebrew Formula for ss-apple.

## Setting up the Homebrew Tap

To publish ss-apple to Homebrew, you need to create a separate repository:

### Step 1: Create a new GitHub repository

Create a new repository named `homebrew-ss` under your GitHub account.

### Step 2: Set up the tap structure

```bash
# Clone your new tap repo
git clone https://github.com/aaa2015/homebrew-ss.git
cd homebrew-ss

# Create Formula directory
mkdir -p Formula

# Copy the formula
cp /path/to/myiosss/homebrew/ss-apple.rb Formula/

# Commit and push
git add .
git commit -m "Add ss-apple formula"
git push
```

### Step 3: Install using Homebrew

Users can now install ss-apple with:

```bash
# Add the tap
brew tap aaa2015/ss

# Install
brew install ss-apple

# Use
ss-apple -tulnp
```

## Updating the Formula

When releasing a new version:

1. Create a new release tag in myiosss repo
2. Get the new SHA256: `curl -sL https://github.com/aaa2015/myiosss/archive/vX.Y.Z.tar.gz | shasum -a 256`
3. Update the formula with new version and sha256
4. Push to homebrew-ss repo

## Alternative: Install directly without tap

Users can also install directly from the formula URL:

```bash
brew install --formula https://raw.githubusercontent.com/aaa2015/homebrew-ss/main/Formula/ss-apple.rb
```

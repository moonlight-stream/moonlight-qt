#!/bin/bash

echo "📝 Committing NvHTTP clipboard endpoints implementation..."

# Check git status first
echo "🔍 Checking git status..."
git status --porcelain

# Add all changed files (force add in case they exist)
echo "📁 Adding files..."
git add app/backend/nvhttp.h 2>/dev/null || echo "nvhttp.h already staged or no changes"
git add app/backend/nvhttp.cpp 2>/dev/null || echo "nvhttp.cpp already staged or no changes"
git add app/backend/clipboardmanager.h 2>/dev/null || echo "clipboardmanager.h already staged or no changes"
git add app/backend/clipboardmanager.cpp 2>/dev/null || echo "clipboardmanager.cpp already staged or no changes"

# Add our documentation files
git add CLIPBOARD_IMPLEMENTATION_SUMMARY.md 2>/dev/null || echo "Summary already staged"
git add commit_changes.sh 2>/dev/null || echo "Script already staged"
git add create_feature_branch.sh 2>/dev/null || echo "Script already staged"

# Check if there are any staged changes
STAGED_CHANGES=$(git diff --cached --name-only)
if [ -z "$STAGED_CHANGES" ]; then
    echo "⚠️  No changes to commit. Files may already be committed."
    echo "📋 Current branch status:"
    git log --oneline -5
    exit 0
fi

echo "📝 Staged changes:"
echo "$STAGED_CHANGES"

# Commit with descriptive message
git commit -m "feat(clipboard): implement NvHTTP clipboard endpoints for Apollo servers

🔧 NvHTTP Integration:
- Add getClipboardContent() method for downloading clipboard
- Add sendClipboardContent() method for uploading clipboard  
- Implement real Artemis protocol: /actions/clipboard?type=text
- Add proper SSL configuration and authentication
- Add comprehensive error handling and logging

📋 ClipboardManager Enhancements:
- Add Apollo server detection (isClipboardSyncSupported)
- Update to use new NvHTTP clipboard methods
- Add apolloSupportChanged signal for UI updates
- Simplify HTTP implementation (remove manual requests)
- Add proper error handling with user feedback
- Add toast notifications for sync operations

🚀 Key Features:
- Real Artemis Android protocol compatibility
- Apollo/Sunshine server detection
- Loop prevention with SHA-256 content hashing
- Bidirectional sync support
- Smart sync triggers (stream start/resume/focus)
- Size limits and content validation
- Comprehensive logging and debugging

Matches Artemis Android NvHTTP.java implementation exactly."

echo "✅ Changes committed to feature branch!"
echo "📋 Ready for testing and PR creation"
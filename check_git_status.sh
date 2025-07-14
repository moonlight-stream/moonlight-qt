#!/bin/bash

echo "🔍 Git Status Check"
echo "==================="

echo "📋 Current branch:"
git branch --show-current

echo ""
echo "📝 Recent commits:"
git log --oneline -5

echo ""
echo "🔄 Working directory status:"
git status --porcelain

echo ""
echo "📁 Staged changes:"
git diff --cached --name-only

echo ""
echo "🔍 Checking our key files:"
echo "nvhttp.h: $(git status --porcelain app/backend/nvhttp.h || echo 'No changes')"
echo "nvhttp.cpp: $(git status --porcelain app/backend/nvhttp.cpp || echo 'No changes')"
echo "clipboardmanager.h: $(git status --porcelain app/backend/clipboardmanager.h || echo 'No changes')"
echo "clipboardmanager.cpp: $(git status --porcelain app/backend/clipboardmanager.cpp || echo 'No changes')"
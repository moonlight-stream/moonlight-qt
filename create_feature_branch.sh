#!/bin/bash

# Create feature branch for NvHTTP clipboard endpoints
echo "🌿 Creating feature branch: feature/nvhttp-clipboard-endpoints"

# Make sure we're on development branch
git checkout development
git pull origin development

# Create and checkout feature branch
git checkout -b feature/nvhttp-clipboard-endpoints

echo "✅ Feature branch created and checked out!"
echo "📋 Ready to implement clipboard endpoints in NvHTTP class"

# Show current branch
git branch --show-current
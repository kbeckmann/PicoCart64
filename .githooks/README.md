# .githooks

To install the git hooks, please run `git config core.hooksPath .githooks` from the root of the repository.

### `pre-commit`

Executes `./sw/scripts/indent.sh --cleanup` before a commit is made. If indentation was fixed, this will be changed in your working copy, i.e. you have to manually review and stage the changes before committing again.

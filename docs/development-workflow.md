# Development workflow

## Branches

`main` is the integration branch. A change starts from current `main` in a
short-lived branch such as `feature/sen66-driver` or
`chore/project-bootstrap`.

## Change lifecycle

1. Create a focused branch.
2. Make one cohesive change with its documentation and tests.
3. Run relevant local checks.
4. Commit using Conventional Commits, for example
   `feat(sen66): add measurement frame parser`.
5. Push the branch and open a draft pull request targeting `main`.
6. Review the diff and checks, then squash-merge it into `main`.
7. Delete the merged feature branch.

## Definition of done

A feature is complete only when it has clear acceptance criteria, suitable unit
or hardware tests, documentation for externally visible behavior, and no
secrets or generated build output in the diff.

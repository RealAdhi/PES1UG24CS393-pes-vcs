Phase 5: Branching and Checkout
Q5.1 (Implementing Checkout): To implement pes checkout <branch>, you must update .pes/HEAD to point to the new branch ref. Then, the working directory must be "wiped" and replaced with the files listed in the target commit's tree. It is complex because you must handle "dirty" files (uncommitted changes) so they aren't accidentally deleted during the swap.

Q5.2 (Detecting Conflicts): A "dirty working directory" is detected by comparing the mtime and size of files in the folder against the values stored in the Index. If they don't match, the file has been modified locally and the checkout should be blocked.

Q5.3 (Detached HEAD): In a detached HEAD state, you are on a specific commit rather than a branch. If you commit here, you create "orphan" commits. To recover them, you would need to find the hash in pes log and manually create a new branch ref (.pes/refs/heads/new-branch) pointing to that hash.

Phase 6: Garbage Collection (GC)
Q6.1 (Algorithm): Use a Mark-and-Sweep algorithm. Starting from all branch refs in refs/heads/, recursively follow every tree and blob hash and mark them as "reachable." Any object in .pes/objects not marked is "dangling" and can be deleted. For 100k commits, you'd likely visit hundreds of thousands of objects.

Q6.2 (Race Conditions): GC is dangerous during a commit because the GC might see a newly written blob as "unreachable" before the commit object that points to it is fully finished and linked to a branch. Git avoids this by only deleting objects older than a certain "grace period" (usually 2 weeks).
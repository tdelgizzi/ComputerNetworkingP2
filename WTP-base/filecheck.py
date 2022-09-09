import filecmp
import sys

file1 = sys.argv[1]
file2 = sys.argv[2]

same = False
filecmp.clear_cache()
same = filecmp.cmp(file1, file2, shallow = False)

if same:
    print("The files are identical")
else:
    print("The files are different")

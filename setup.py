import sys

if sys.version_info >= (3, 0):
    import setup3

    print(setup3.build_id)
else:
    import setup2

    print(setup2.build_id)

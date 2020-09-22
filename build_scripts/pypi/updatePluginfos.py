
import argparse, base64, csv, fnmatch, hashlib, io, json, os, zipfile

# TODO comment and usage

def get_hash(data):
    # based on the rewrite_record function in auditwheel
    # https://github.com/pypa/auditwheel/blob/67ba9ff9c43fc6d871ee3e7e300125fe6bf282f3/auditwheel/wheeltools.py#L45
    # also, the encoding helpers found here
    # https://github.com/pypa/wheel/blob/master/src/wheel/util.py#L26
    digest = hashlib.sha256(data).digest()
    hashval = 'sha256=' + \
            base64.urlsafe_b64encode(digest).rstrip(b'=').decode('utf-8')
    size = len(data)
    return (hashval, size)


def update_pluginfo(contents, new_lib_names, new_pluginfo_hashes):
    # some USD json files begin with python style comments, and those aren't
    # legal json. For our purposes we'll strip them.
    while contents[0] == ord('#'):
        line_end = contents.find(ord('\n'))
        contents = contents[line_end+1:]

    json_doc = json.loads(contents.decode('utf-8'))
    changed = False
    for p in json_doc.get("Plugins", []):
        if "LibraryPath" in p:
            libpath = p["LibraryPath"]
            _, libname = os.path.split(libpath)
            libname, _ = os.path.splitext(libname)
            # libname is now like libsdf or libtf
            for newname in new_lib_names:
                if f'libs/{libname}-' in newname:
                    # found a new location for this lib
                    # TODO, this is assuming the relative path structure that
                    # we currently get. I think this should work for the way
                    # our wheels are currently built, and the way USD is
                    # currently setting these paths internally. If either
                    # changes this will break.
                    p['LibraryPath'] = os.path.join('../../../', newname)
                    changed = True
                    break

    if changed:
        return json.dumps(json_doc, indent=4).encode('utf-8')

    return contents


def update_record(contents, new_pluginfo_hashes):

    result = io.StringIO()
    csv_writer = csv.writer(result)
    csv_reader = csv.reader(io.StringIO(contents.decode('utf-8')))
    for row in csv_reader:
        if len(row) > 0 and row[0] in new_pluginfo_hashes:
            sha, size = new_pluginfo_hashes[row[0]]
            row[1] = sha
            row[2] = size
        csv_writer.writerow(row)
    return result.getvalue()


def parse_command_line():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_file", type=str, 
                        help="The input wheel archive to be updated")
    parser.add_argument("output_file", type=str, 
                        help="The output wheel archive to write to")
    args = parser.parse_args()
    return args.input_file, args.output_file


def main():

    input_file, output_file = parse_command_line()

    if not os.path.exists(input_file):
        raise Exception("no such file " + input_file)

    # Find the archive
    # Make a temp dir
    # unzip
    # read each pluginfo and update it
    # zip back up

    with zipfile.ZipFile(input_file, 'r') as input_wheel:
        with zipfile.ZipFile(output_file, 'w', compression=input_wheel.compression) as output_wheel:
            # This seems blank, but can't hurt to keep it
            output_wheel.comment = input_wheel.comment

            new_lib_names = fnmatch.filter(input_wheel.namelist(), '*/lib*.so')
            new_pluginfo_hashes = {}

            for file_info in input_wheel.infolist():
                if fnmatch.fnmatch(file_info.filename, '*/plugInfo.json'):
                    print(f'processing: {file_info.filename}')
                    data = input_wheel.read(file_info)
                    contents = update_pluginfo(data, new_lib_names, new_pluginfo_hashes)

                    # update hash
                    new_pluginfo_hashes[file_info.filename] = get_hash(contents)

                    output_wheel.writestr(file_info, contents)
                elif fnmatch.fnmatch(file_info.filename, '*/RECORD'):
                    # contains hashes of every file. Luckily, it comes at the end of the
                    # archive, so I can use the hashes I precomputed.
                    # TODO this is fragile as f. Instead we could extract everything to a
                    # temp directory, do all our changes there, and then zip up in the
                    # expected way. If we do that we could conceivably use the existing
                    # helper functions in wheel and auditwheel, which are pretty nice
                    # and straightforward
                    print(f'updating hashes in {file_info.filename}')
                    data = input_wheel.read(file_info)
                    contents = update_record(data, new_pluginfo_hashes)
                    output_wheel.writestr(file_info, contents)
                else:
                    print(f'copying: {file_info.filename}')
                    # writestr also writes binary if passed a bytes instance
                    output_wheel.writestr(file_info, input_wheel.read(file_info))

if __name__ == '__main__':
    main()

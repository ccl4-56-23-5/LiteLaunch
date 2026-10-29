"""Exercise the shipped worker protocol using disposable files only."""
import sys, subprocess, struct, json, tempfile, pathlib, hashlib, zipfile, zlib, time
exe=pathlib.Path(sys.argv[1]).resolve()
root=pathlib.Path(tempfile.mkdtemp(prefix='LL_ADVANCED_QA_',dir=exe.parent))
p=subprocess.Popen([str(exe)],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
checks=0
def invoke(options,names=('Test.txt',),paths=None,folders=None,fail=False):
    options={k:str(v) for k,v in options.items()}
    rows=[]
    for i,name in enumerate(names):
        folder=bool(folders and folders[i]);path=paths[i] if paths else root/name
        ext='' if folder or '.' not in name or name.startswith('.') else '.'+name.rsplit('.',1)[1]
        rows.append([str(path),name,name,'1' if folder else '0',ext])
    strings=[json.dumps(options,ensure_ascii=False),'',json.dumps(rows,ensure_ascii=False)]
    p.stdin.write(struct.pack('<i',1024)+b''.join(struct.pack('<i',len(s.encode()))+s.encode() for s in strings));p.stdin.flush()
    status=struct.unpack('<i',p.stdout.read(4))[0];length=struct.unpack('<i',p.stdout.read(4))[0];text=p.stdout.read(length).decode()
    if fail:
        assert not status, (options,text)
        return text
    assert status,(options,text)
    return json.loads(text)
def expect(label,o,want,names=('Test.txt',),**kwargs):
    global checks
    result=invoke(o,names,**kwargs);actual=[r[0] for r in result];assert actual==want,(label,actual,want);checks+=1;print('PASS',label,flush=True)
try:
    expect('insert metadata current name',{'type':'Insert','text':'{name}-','where':'prefix'},['Test-Test.txt'])
    expect('insert anchor absent unchanged',{'type':'Insert','text':'X','where':'before','anchor':'absent'},['Test.txt'])
    expect('Unicode delete positions',{'type':'Delete','position':2,'count':1},['AB.txt'],('A🙂B.txt',))
    expect('delete delimiters',{'type':'Delete','from':'delimiter','fromText':'[','until':'delimiter','untilText':']'},['AB.txt'],('A[x]B.txt',))
    expect('clear stem',{'type':'Delete','clear':1},['.txt'])
    expect('remove whole words',{'type':'Remove','find':'a','whole':1},[' beta ab.txt'],('a beta ab.txt',))
    expect('replace wildcards',{'type':'Replace','find':'a[0-9]?','wildcards':1,'replace':'X'},['X X.txt'],('a12 a34.txt',))
    expect('replace last',{'type':'Replace','find':'a','replace':'b','occurrence':'last'},['aab.txt'],('aaa.txt',))
    expect('regex groups',{'type':'RegEx','find':'(a)([0-9]+)','replace':'$2-$1'},['12-a.txt'],('a12.txt',))
    expect('rearrange',{'type':'Rearrange','splitBy':' - ','pattern':'$2 $1 $-1'},['B A B.txt'],('A - B.txt',))
    expect('extension',{'type':'Extension','extension':'png'},['Test.png'])
    expect('extension ignores folders',{'type':'Extension','extension':'png'},['Folder.abc'],('Folder.abc',),folders=[True])
    expect('strip Unicode ranges',{'type':'Strip','ranges':'1F600-1F64F'},['AB.txt'],('A🙂B.txt',))
    expect('case forced fragments',{'type':'Case','caseMode':'title','forceCase':'iPhone,USB','extensionCase':'lower'},['iPhone USB.jpg'],('IPHONE usb.JPG',))
    expect('serialize negative step and repeat',{'type':'Serialize','start':5,'step':-2,'repeat':2,'width':3,'where':'replace'},['005.txt','005.txt','003.txt'],('A.txt','B.txt','C.txt'))
    expect('serialize reset count',{'type':'Serialize','resetEvery':2,'where':'replace'},['1.txt','2.txt','1.txt'],('A.txt','B.txt','C.txt'))
    for system,start,want in [('letters',27,'ba'),('roman',19,'XIX'),('music',1,'C0'),('chinese',10001,'一万零一'),('alphabetic',3,'ba'),('numeric',3,'bb')]:
        expect('serialize '+system,{'type':'Serialize','system':system,'start':start,'alphabet':'ab','where':'replace'},[want+'.txt'])
    expect('padding number',{'type':'Padding','width':3},['A007.txt'],('A7.txt',))
    expect('padding strip zeros',{'type':'Padding','padMode':'remove'},['A7.txt'],('A0007.txt',))
    expect('padding text',{'type':'Padding','padMode':'text','padText':'X','width':6},['XXTest.txt'])
    expect('cleanup nesting and spaces',{'type':'CleanUp','brackets':'()[]{}'},['A B.txt'],('A_[x(y)]_B.txt',))
    expect('custom transliteration',{'type':'Translit','map':'中国=CN\n北京=BJ'},['CN-BJ.txt'],('中国-北京.txt',))
    expect('ICU Chinese transliteration',{'type':'Translit','alphabetMap':'Any-Latin; Latin-ASCII'},['zhong guo.txt'],('中国.txt',))
    expect('ICU simplified Chinese',{'type':'Translit','alphabetMap':'Traditional-Simplified'},['软件.txt'],('軟體.txt',)) if False else None
    expect('date format and adjustment',{'type':'ReformatDate','inputFormat':'yyyy-mm-dd','outputFormat':'yyyymmdd','adjust':'0,0,1,0,0,0,0'},['Photo-20240301.txt'],('Photo-2024-02-29.txt',))
    expect('name list short retains remainder',{'type':'UserInput','list':'New'},['New.txt','B.txt'],('A.txt','B.txt'))
    expect('CSV quoted mappings',{'type':'Mapping','mapping':'"A,B","X,Y"'},['X,Y.txt'],('A,B.txt',))
    r=invoke({'type':'Randomize','length':1,'randomChars':'digits','where':'replace'},tuple(str(i)+'.txt' for i in range(10)));assert len({x[0] for x in r})==10;checks+=1;print('PASS random uniqueness')
    f=root/'abc.txt';f.write_bytes(b'abc')
    expect('hash SHA256',{'type':'Metadata','pattern':':File_SHA256:'},[hashlib.sha256(b'abc').hexdigest()+'.txt'],paths=[f])
    expect('hash CRC32',{'type':'Metadata','pattern':':File_CRC32:'},[format(zlib.crc32(b'abc'),'08x')+'.txt'],paths=[f])
    png=root/'元数据.png';w,h=3,2
    def chunk(t,data):return struct.pack('>I',len(data))+t+data+struct.pack('>I',zlib.crc32(t+data))
    png.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress((b'\x00'+b'\xff\x00\x00'*w)*h))+chunk(b'IEND',b''))
    expect('real PNG metadata',{'type':'Metadata','pattern':':Image_Width:x:Image_Height:'},['3x2.png'],('元数据.png',),paths=[png])
    expect('content extension detect',{'type':'Extension','detect':1},['Wrong.png'],('Wrong.bin',),paths=[png])
    doc=root/'文档.docx'
    with zipfile.ZipFile(doc,'w') as z:z.writestr('docProps/core.xml','<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:creator>作者</dc:creator><dc:title>标题</dc:title></cp:coreProperties>')
    expect('real OOXML author title',{'type':'Metadata','pattern':':Document_Author:-:Document_Title:'},['作者-标题.docx'],('文档.docx',),paths=[doc])
    expect('missing metadata fallback',{'type':'Metadata','pattern':':EXIF_Model:','missing':'fallback','fallback':'Unknown'},['Unknown.png'],('元数据.png',),paths=[png])
    exif=b'Exif\0\0'+bytes((0x49,0x49,0x2a,0,8,0,0,0,1,0,0x69,0x87,4,0,1,0,0,0,26,0,0,0,0,0,0,0,1,0,3,0x90,2,0,20,0,0,0,44,0,0,0,0,0,0,0))+b'2020:01:02 03:04:05\0'
    jpeg=root/'photo.jpg';jpeg.write_bytes(b'\xff\xd8\xff\xe1'+struct.pack('>H',len(exif)+2)+exif+b'\xff\xd9')
    expect('EXIF date and legacy .NET time pattern',{'type':'Metadata','pattern':'{taken:yyyyMMdd-HHmmss}'},['20200102-030405.jpg'],('photo.jpg',),paths=[jpeg])
    def id3frame(key,value):
        data=b'\x03'+value.encode();return key.encode()+struct.pack('>I',len(data))+b'\0\0'+data
    tags=id3frame('TIT2','曲名')+id3frame('TPE1','歌手');n=len(tags);mp3=root/'song.mp3';mp3.write_bytes(b'ID3\x03\x00\x00'+bytes(((n>>21)&127,(n>>14)&127,(n>>7)&127,n&127))+tags)
    expect('ID3 audio title and artist',{'type':'Metadata','pattern':':ID3_Title:-:Audio_Artist:'},['曲名-歌手.mp3'],('song.mp3',),paths=[mp3])
    info=b'd6:lengthi3e4:name4:teste';torrent=root/'sample.torrent';torrent.write_bytes(b'd4:info'+info+b'e')
    expect('torrent name size and info hash',{'type':'Metadata','pattern':':Torrent_Name:-:Torrent_Size:-:Torrent_Hash:'},['test-3-'+hashlib.sha1(info).hexdigest()+'.torrent'],('sample.torrent',),paths=[torrent])
    apk=root/'sample.apk'
    with zipfile.ZipFile(apk,'w') as z:z.writestr('AndroidManifest.xml','<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="org.example.test" android:versionName="1.2.3"><application android:label="测试"/></manifest>')
    expect('APK structured metadata',{'type':'Metadata','pattern':':APK_Package:-:APK_Label:-:APK_VersionName:'},['org.example.test-测试-1.2.3.apk'],('sample.apk',),paths=[apk])
    eml=root/'message.eml';eml.write_bytes(b'Subject: =?UTF-8?B?5rWL6K+V?=\r\nFrom: Example <a@example.com>\r\n\r\nContent')
    expect('mail MIME subject',{'type':'Metadata','pattern':':Email_Subject:'},['测试.eml'],('message.eml',),paths=[eml])
    invoke({'type':'Metadata','pattern':':EXIF_Model:'},('元数据.png',),paths=[png],fail=True);checks+=1;print('PASS missing metadata stops')
    expect('Pascal numeric Unicode and loops',{'type':'PascalScript','script':"var i: Integer; begin for i := 1 to 2 do FileName := FileName + '中'; FileName := IntToStr(Counter) + '-' + FileName; end."},['1-Test中中.txt'])
    expect('Pascal metadata host',{'type':'PascalScript','script':"begin FileName := CalculateMetaTag(FilePath, 'File_Name'); end."},['abc.txt'],paths=[f])
    expect('Pascal wide string helper',{'type':'PascalScript','script':"begin FileName := WideCopy('中文AB',1,2) + WideReverseString('A🙂B'); end."},['中文B🙂A.txt'])
    expect('Pascal native Int64 and date helpers',{'type':'PascalScript','script':"begin FileName := Int64ToStr(1234567890123) + '-' + FormatDateTime('yyyy', IncYear(ScanDateTime('yyyy-mm-dd', '2024-02-29'),1)); end."},['1234567890123-2025.txt'])
    expect('Pascal regex substitution flag',{'type':'PascalScript','script':"begin FileName := ReplaceRegEx(FileName, '(Test)', '$1X', True, False); end."},['$1X.txt'])
    invoke({'type':'PascalScript','script':"begin BadSyntax ??? end."},fail=True);checks+=1;print('PASS Pascal compile failure')
    invoke({'type':'PascalScript','script':"begin WriteTextFile('bad','bad'); end."},fail=True);checks+=1;print('PASS Pascal file IO disabled')
    begin=time.monotonic();invoke({'type':'PascalScript','script':"begin while True do begin FileName := FileName; end; end."},fail=True);assert time.monotonic()-begin<8;checks+=1;print('PASS Pascal timeout')
    print(json.dumps({'passed':checks,'root':str(root),'worker_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()},ensure_ascii=True))
finally:
    p.stdin.write(struct.pack('<i',-1));p.stdin.flush();p.wait(timeout=5)

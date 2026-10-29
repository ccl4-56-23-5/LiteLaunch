program PascalRules;
{$mode delphi}{$H+}{$codepage utf8}
uses SysUtils, Classes, Variants, Windows, Math, DateUtils, fpjson, jsonparser,
  uPSComponent, uPSCompiler, uPSRuntime, uPSUtils, uPSC_std, uPSR_std, uPSC_classes, uPSR_classes;

type
  TScriptHost=class
    AllowIO:Boolean;
    Deadline:QWord;
    procedure Compile(Sender:TPSScript);
    procedure ImportCompile(Sender:TObject; C:TPSPascalCompiler);
    procedure ImportRuntime(Sender:TObject; E:TPSExec; C:TPSRuntimeClassImporter);
    procedure Tick(Sender:TObject);
  end;
var Host:TScriptHost;

function Remote(const Op:UTF8String; const Args:array of WideString):WideString;
var O,R:TJSONObject; A:TJSONArray; Line:UTF8String; I:Integer;
begin
  O:=TJSONObject.Create;A:=TJSONArray.Create;
  try
    O.Add('call',Op);for I:=0 to High(Args) do A.Add(UTF8Encode(Args[I]));O.Add('args',A);
    WriteLn(O.AsJSON);Flush(Output);if EOF(Input) then raise Exception.Create('Host disconnected');ReadLn(Line);
    R:=GetJSON(Line) as TJSONObject;try if R.Get('error','')<>'' then raise Exception.Create(R.Get('error',''));Result:=UTF8Decode(R.Get('value',''));finally R.Free;end;
  finally O.Free;end;
end;
function WUpper(const S:WideString):WideString;begin Result:=WideUpperCase(S);end;
function WLower(const S:WideString):WideString;begin Result:=WideLowerCase(S);end;
function WTrim(const S:WideString):WideString;begin Result:=Trim(S);end;
function WIntToStr(N:Integer):WideString;begin Result:=IntToStr(N);end;
function WStrToInt(const S:WideString):Integer;begin Result:=StrToInt64(S);end;
function WExtractName(const S:WideString):WideString;begin Result:=ExtractFileName(S);end;
function WExtractPath(const S:WideString):WideString;begin Result:=ExtractFilePath(S);end;
function WExtractExt(const S:WideString):WideString;begin Result:=ExtractFileExt(S);end;
function WChangeExt(const S,E:WideString):WideString;begin Result:=ChangeFileExt(S,E);end;
function WReplace(const S,F,R:WideString):WideString;begin Result:=StringReplace(S,F,R,[rfReplaceAll]);end;
function WMeta(const P,T:WideString):WideString;begin Result:=Remote('meta',[P,T]);end;
function WRegex(const S,F,R:WideString; Sensitive:Boolean; Substitution:Boolean):WideString;var A,B:WideString;begin if Sensitive then A:='1' else A:='0';if Substitution then B:='1' else B:='0';Result:=Remote('regex',[S,F,R,A,B]);end;
function WRead(const P:WideString):WideString;begin Result:=Remote('read',[P]);end;
function WFileExists(const P:WideString):Boolean;begin Result:=GetFileAttributesW(PWideChar(P))<>INVALID_FILE_ATTRIBUTES;end;
function WDirectoryExists(const P:WideString):Boolean;var A:DWORD;begin A:=GetFileAttributesW(PWideChar(P));Result:=(A<>INVALID_FILE_ATTRIBUTES) and ((A and FILE_ATTRIBUTE_DIRECTORY)<>0);end;
function WFileSize(const P:WideString):Int64;var S:WideString;begin S:=Remote('size',[P]);Result:=StrToInt64(S);end;
function WFormatDate(const F:WideString; D:TDateTime):WideString;begin Result:=FormatDateTime(F,D);end;
function WNow:TDateTime;begin Result:=Now;end;
function WDate:TDateTime;begin Result:=Date;end;
procedure WWrite(const P,S:WideString);begin if not Host.AllowIO then raise Exception.Create('Script IO is disabled');Remote('write',[P,S]);end;
function WDelete(const P:WideString):Boolean;begin if not Host.AllowIO then raise Exception.Create('Script IO is disabled');Result:=Remote('delete',[P])='1';end;
function WRename(const A,B:WideString):Boolean;begin if not Host.AllowIO then raise Exception.Create('Script IO is disabled');Result:=Remote('rename',[A,B])='1';end;
function WCreateDir(const P:WideString):Boolean;begin if not Host.AllowIO then raise Exception.Create('Script IO is disabled');Result:=Remote('mkdir',[P])='1';end;
function WExec(const FileName,Parameters:WideString):WideString;begin if not Host.AllowIO then raise Exception.Create('Script IO is disabled');Result:=Remote('exec',[FileName,Parameters]);end;
procedure TScriptHost.ImportCompile(Sender:TObject; C:TPSPascalCompiler);
begin C.UTF8Decode:=True;SIRegister_Std(C);SIRegister_Classes(C,AllowIO);C.AddTypeS('TDateTime','Double');end;
procedure TScriptHost.ImportRuntime(Sender:TObject; E:TPSExec; C:TPSRuntimeClassImporter);
begin RIRegister_Std(C);RIRegister_Classes(C,AllowIO);end;
{$I pascal_strings.inc}
procedure TScriptHost.Compile(Sender:TPSScript);
begin
  RegisterCompatibility(Sender);
  Sender.AddRegisteredVariable('FileName','WideString');Sender.AddRegisteredVariable('FilePath','WideString');Sender.AddRegisteredVariable('Counter','Integer');Sender.AddRegisteredVariable('FileIndex','Integer');Sender.AddRegisteredVariable('FileCount','Integer');
  Sender.AddFunction(@WUpper,'function WideUpperCase(const S: WideString): WideString;');
  Sender.AddFunction(@WLower,'function WideLowerCase(const S: WideString): WideString;');
  Sender.AddFunction(@WExtractName,'function ExtractFileName(const S: WideString): WideString;');
  Sender.AddFunction(@WExtractPath,'function ExtractFilePath(const S: WideString): WideString;');
  Sender.AddFunction(@WExtractExt,'function ExtractFileExt(const S: WideString): WideString;');
  Sender.AddFunction(@WChangeExt,'function ChangeFileExt(const S,E: WideString): WideString;');
  Sender.AddFunction(@WReplace,'function WideReplaceStr(const S,F,R: WideString): WideString;');
  Sender.AddFunction(@WReplace,'function ReplaceStr(const S,F,R: WideString): WideString;');
  Sender.AddFunction(@WMeta,'function CalculateMetaTag(const FilePath, MetaTag: WideString): WideString;');
  Sender.AddFunction(@WRegex,'function ReplaceRegEx(const Input, Find, Replace: WideString; CaseSensitive, UseSubstitution: Boolean): WideString;');
  Sender.AddFunction(@WRegex,'function RegExReplace(const Input, Find, Replace: WideString; CaseSensitive, UseSubstitution: Boolean): WideString;');
  Sender.AddFunction(@WFileExists,'function FileExists(const Path: WideString): Boolean;');
  Sender.AddFunction(@WDirectoryExists,'function DirectoryExists(const Path: WideString): Boolean;');
  Sender.AddFunction(@WFileSize,'function FileSize(const Path: WideString): Int64;');
  Sender.AddFunction(@WRead,'function ReadTextFile(const Path: WideString): WideString;');
  Sender.AddFunction(@WFormatDate,'function FormatDateTime(const Format: WideString; Value: TDateTime): WideString;');
  Sender.AddFunction(@WNow,'function Now: TDateTime;');Sender.AddFunction(@WDate,'function Date: TDateTime;');
  if AllowIO then begin
    Sender.AddFunction(@WWrite,'procedure WriteTextFile(const Path, Content: WideString);');
    Sender.AddFunction(@WDelete,'function DeleteFile(const Path: WideString): Boolean;');
    Sender.AddFunction(@WRename,'function RenameFile(const OldName, NewName: WideString): Boolean;');
    Sender.AddFunction(@WCreateDir,'function CreateDir(const Path: WideString): Boolean;');
    Sender.AddFunction(@WExec,'function ExecConsoleApp(const FileName, Parameters: WideString): WideString;');
  end;
end;
procedure TScriptHost.Tick(Sender:TObject);
begin if GetTickCount64>Deadline then raise Exception.Create('Pascal Script exceeded 2 seconds');end;

var InputLine:UTF8String;Request,Answer:TJSONObject;Rows,Names:TJSONArray;Script:TPSScript;I,J:Integer;ErrorText:UTF8String;Row:TJSONObject;
begin
  SetTextCodePage(Input,65001);SetTextCodePage(Output,65001);SetMultiByteConversionCodePage(65001);
  Request:=nil;Answer:=TJSONObject.Create;Host:=TScriptHost.Create;Script:=TPSScript.Create(nil);
  try
    try
      ReadLn(InputLine);Request:=GetJSON(InputLine) as TJSONObject;Host.AllowIO:=Request.Get('allowIO',False);
      Script.OnCompile:=Host.Compile;Script.OnCompImport:=Host.ImportCompile;Script.OnExecImport:=Host.ImportRuntime;Script.OnLine:=Host.Tick;
      Script.Script.Text:=Request.Get('script','');
      if not Script.Compile then begin ErrorText:='';for J:=0 to Script.CompilerMessageCount-1 do ErrorText:=ErrorText+Script.CompilerErrorToStr(J)+#10;raise Exception.Create(ErrorText);end;
      Rows:=Request.Arrays['rows'];Names:=TJSONArray.Create;Answer.Add('names',Names);
      for I:=0 to Rows.Count-1 do begin
        Row:=Rows.Objects[I];
        PPSVariantWString(Script.GetVariable('FILENAME')).Data:=UTF8Decode(Row.Get('name',''));
        PPSVariantWString(Script.GetVariable('FILEPATH')).Data:=UTF8Decode(Row.Get('path',''));
        PPSVariantS32(Script.GetVariable('COUNTER')).Data:=I+1;PPSVariantS32(Script.GetVariable('FILEINDEX')).Data:=I+1;PPSVariantS32(Script.GetVariable('FILECOUNT')).Data:=Rows.Count;
        Host.Deadline:=GetTickCount64+2000;
        if not Request.Get('compileOnly',False) then if not Script.Execute then raise Exception.Create(Script.ExecErrorToString);
        Names.Add(UTF8Encode(PPSVariantWString(Script.GetVariable('FILENAME')).Data));
      end;
      Answer.Add('done',True);
    except on E:Exception do begin Answer.Add('done',True);Answer.Add('error',UTF8String(E.Message));end;end;
    WriteLn(Answer.AsJSON);Flush(Output);
  finally Script.Free;Host.Free;Request.Free;Answer.Free;end;
end.

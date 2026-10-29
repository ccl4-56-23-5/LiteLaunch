using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.Collections;
using System.Diagnostics;
using System.Globalization;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

internal static class RenamerPascal {
    internal static string[] Run(string script,List<string[]>rows,List<string>subjects,bool allowIO,bool compileOnly){
        string exe=RenamerMetadata.ToolFile("PascalRules.exe");var start=new ProcessStartInfo(exe);start.UseShellExecute=false;start.CreateNoWindow=true;start.RedirectStandardInput=true;start.RedirectStandardOutput=true;start.RedirectStandardError=true;start.StandardOutputEncoding=new UTF8Encoding(false,true);
        using(var process=Process.Start(start))using(var input=new StreamWriter(process.StandardInput.BaseStream,new UTF8Encoding(false))){
            process.ErrorDataReceived+=delegate{};process.BeginErrorReadLine();input.AutoFlush=true;var files=new List<object>();for(int i=0;i<rows.Count;i++)files.Add(new{path=rows[i][0],name=subjects[i]});input.WriteLine(RenamerRules.Json.Serialize(new{script=script,rows=files,allowIO=allowIO,compileOnly=compileOnly}));DateTime deadline=DateTime.UtcNow.AddSeconds(Math.Min(110,5+rows.Count*3));
            try{while(true){Task<string> next=process.StandardOutput.ReadLineAsync();int left=(int)Math.Max(0,(deadline-DateTime.UtcNow).TotalMilliseconds);if(!next.Wait(left))throw new TimeoutException("Pascal脚本超时，操作已停止。");string line=next.Result;if(line==null)throw new IOException("Pascal脚本组件提前退出。");var message=RenamerRules.Json.Deserialize<Dictionary<string,object>>(line);if(message.ContainsKey("done")){string error=RenamerRules.Get(message,"error");if(error.Length>0)throw new ArgumentException("Pascal脚本："+error);object[] names=AsArray(message["names"]);if(names.Length!=rows.Count)throw new InvalidDataException("脚本名称行数不一致。");return Array.ConvertAll(names,delegate(object v){return Convert.ToString(v,CultureInfo.InvariantCulture);});}
                string op=RenamerRules.Get(message,"call");object[] args=AsArray(message["args"]);var values=Array.ConvertAll(args,delegate(object v){return Convert.ToString(v,CultureInfo.InvariantCulture);});try{if(op=="meta"){int index=rows.FindIndex(delegate(string[] row){return String.Equals(row[0],values[0],StringComparison.OrdinalIgnoreCase);});if(index>=0){RenamerMetadata.CurrentIndex=index;RenamerMetadata.CurrentName=subjects[index];}}string value=Call(op,values,allowIO);input.WriteLine(RenamerRules.Json.Serialize(new{value=value}));}catch(Exception e){input.WriteLine(RenamerRules.Json.Serialize(new{error=e.Message}));}
            }}finally{input.Close();if(!process.WaitForExit(500))process.Kill();}
        }
    }
    private static object[] AsArray(object value){var a=value as object[];return a??((ArrayList)value).ToArray();}
    private static string Call(string op,string[]a,bool allowIO){
        if(op=="meta")return RenamerMetadata.Value(a[0],a[1],new Dictionary<string,object>());
        if(op=="regex"){var regex=RenamerRules.Rx(a[1],a[3]=="1"?RegexOptions.None:RegexOptions.IgnoreCase);return a[4]=="1"?regex.Replace(a[0],a[2]):regex.Replace(a[0],delegate(Match m){return a[2];});}
        if(op=="size")return new FileInfo(a[0]).Length.ToString(CultureInfo.InvariantCulture);
        if(op=="read"){if(new FileInfo(a[0]).Length>4*1024*1024)throw new IOException("文本文件超过4MB。");return File.ReadAllText(a[0],new UTF8Encoding(false,true));}
        if(!allowIO)throw new InvalidOperationException("此脚本未开启文件/外部程序操作。");
        if(op=="write"){File.WriteAllText(a[0],a[1],new UTF8Encoding(false));return "";}
        if(op=="delete"){File.Delete(a[0]);return "1";}
        if(op=="rename"){File.Move(a[0],a[1]);return "1";}
        if(op=="mkdir"){Directory.CreateDirectory(a[0]);return "1";}
        if(op=="exec"){
            var start=new ProcessStartInfo(a[0],a[1]){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};using(var process=Process.Start(start)){var output=process.StandardOutput.ReadToEndAsync();var error=process.StandardError.ReadToEndAsync();if(!process.WaitForExit(10000)){process.Kill();throw new TimeoutException("外部程序超过10秒。");}Task.WaitAll(output,error);if(output.Result.Length>4*1024*1024)throw new IOException("外部程序结果过长。");if(process.ExitCode!=0)throw new IOException("外部程序退出码："+process.ExitCode+" "+error.Result);return output.Result;}
        }
        throw new ArgumentException("未知Pascal宿主调用："+op);
    }
}

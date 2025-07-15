package com.nobody.demo;
import android.content.Context;
import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.zip.*;
public class ApkExtractor {
    public static boolean extractLibFromApk(Context context, String apkPath, String targetDir) {
        try {
            File destDir = new File(targetDir);
            if (!destDir.exists() && !destDir.mkdirs()) {
                return false;
            }
            try (ZipInputStream zipIn = new ZipInputStream(Files.newInputStream(Paths.get(apkPath)))) {
                ZipEntry entry;
                while ((entry = zipIn.getNextEntry()) != null) {
                    String entryName = entry.getName();
                    if (entryName.startsWith("lib/") && !entry.isDirectory()) {
                        String fileName = entryName.substring(4);
                        File outputFile = new File(destDir, fileName);
                        // 创建父目录
                        File parent = outputFile.getParentFile();
                        if (!parent.exists() && !parent.mkdirs()) {
                            return false;
                        }
                        // 写入文件
                        try (BufferedOutputStream bos = new BufferedOutputStream(Files.newOutputStream(outputFile.toPath()))) {
                            byte[] buffer = new byte[4096];
                            int bytesRead;
                            while ((bytesRead = zipIn.read(buffer)) != -1) {
                                bos.write(buffer, 0, bytesRead);
                            }
                        }
                    }
                    zipIn.closeEntry();
                }
            }
            return true;
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }
}

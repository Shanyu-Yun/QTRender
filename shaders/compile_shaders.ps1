# 编译着色器脚本
# 源码目录: shaders/code
# 输出目录: shaders/spv

# 确保输出目录存在
$spvDir = Join-Path $PSScriptRoot "spv"
if (-not (Test-Path $spvDir)) {
    New-Item -ItemType Directory -Path $spvDir | Out-Null
    Write-Host "创建输出目录: spv/" -ForegroundColor Green
}

$codeDir = Join-Path $PSScriptRoot "code"
if (-not (Test-Path $codeDir)) {
    Write-Host "错误: 源码目录不存在: code/" -ForegroundColor Red
    exit 1
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "开始编译着色器..." -ForegroundColor Cyan
Write-Host "源码目录: code/" -ForegroundColor Yellow
Write-Host "输出目录: spv/" -ForegroundColor Yellow
Write-Host "========================================`n" -ForegroundColor Cyan

# 获取所有着色器文件（.vert, .frag, .comp, .geom, .tesc, .tese）
$shaderExtensions = @("*.vert", "*.frag", "*.comp", "*.geom", "*.tesc", "*.tese")
$shaderFiles = @()

foreach ($ext in $shaderExtensions) {
    $files = Get-ChildItem -Path $codeDir -Filter $ext -File
    $shaderFiles += $files
}

if ($shaderFiles.Count -eq 0) {
    Write-Host "警告: 在 code/ 目录下未找到任何着色器文件" -ForegroundColor Yellow
    Write-Host "支持的扩展名: .vert, .frag, .comp, .geom, .tesc, .tese" -ForegroundColor Yellow
    exit 0
}

Write-Host "找到 $($shaderFiles.Count) 个着色器文件`n" -ForegroundColor Green

$successCount = 0
$failCount = 0
$compiledFiles = @()

foreach ($file in $shaderFiles) {
    $inputPath = $file.FullName
    $outputFileName = $file.Name + ".spv"
    $outputPath = Join-Path $spvDir $outputFileName
    
    Write-Host "[$($successCount + $failCount + 1)/$($shaderFiles.Count)] 编译: $($file.Name) -> spv/$outputFileName" -ForegroundColor Yellow
    
    # 执行 glslc 编译，指定 Vulkan 1.3 目标环境
    glslc --target-env=vulkan1.3 $inputPath -o $outputPath 2>&1 | Out-String | Write-Host
    
    if ($LASTEXITCODE -eq 0) {
        $successCount++
        $compiledFiles += "  ✓ spv/$outputFileName"
        Write-Host "  成功" -ForegroundColor Green
    }
    else {
        $failCount++
        Write-Host "  失败" -ForegroundColor Red
    }
    Write-Host ""
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "编译统计:" -ForegroundColor Cyan
Write-Host "  总计: $($shaderFiles.Count)" -ForegroundColor White
Write-Host "  成功: $successCount" -ForegroundColor Green
Write-Host "  失败: $failCount" -ForegroundColor $(if ($failCount -gt 0) { "Red" } else { "White" })
Write-Host "========================================" -ForegroundColor Cyan

if ($successCount -gt 0) {
    Write-Host "`n生成的 SPIR-V 文件:" -ForegroundColor Cyan
    foreach ($file in $compiledFiles) {
        Write-Host $file -ForegroundColor Yellow
    }
}

if ($failCount -gt 0) {
    Write-Host "`n部分着色器编译失败，请检查错误信息" -ForegroundColor Red
    exit 1
}
else {
    Write-Host "`n所有着色器编译成功！" -ForegroundColor Green
    exit 0
}

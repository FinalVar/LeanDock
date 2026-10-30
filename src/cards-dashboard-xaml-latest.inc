// Native XAML only. The Dock's existing material is the single background.
static constexpr wchar_t DashboardXaml[]=LR"xaml(
<Border xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
 x:Name="LeanDockCard" Height="44" Background="Transparent" HorizontalAlignment="Right" VerticalAlignment="Center">
 <Border.Resources><ResourceDictionary><ResourceDictionary.ThemeDictionaries>
  <ResourceDictionary x:Key="Default"><SolidColorBrush x:Key="LeanDockUploadBrush" Color="#FF8BE0BD"/><SolidColorBrush x:Key="LeanDockDownloadBrush" Color="#FF91CFFF"/></ResourceDictionary>
  <ResourceDictionary x:Key="Light"><SolidColorBrush x:Key="LeanDockUploadBrush" Color="#FF197F69"/><SolidColorBrush x:Key="LeanDockDownloadBrush" Color="#FF2D69A3"/></ResourceDictionary>
  <ResourceDictionary x:Key="HighContrast"><SolidColorBrush x:Key="LeanDockUploadBrush" Color="{ThemeResource SystemColorWindowTextColor}"/><SolidColorBrush x:Key="LeanDockDownloadBrush" Color="{ThemeResource SystemColorWindowTextColor}"/></ResourceDictionary>
 </ResourceDictionary.ThemeDictionaries></ResourceDictionary></Border.Resources>
 <Border.RenderTransform><TranslateTransform X="17"/></Border.RenderTransform>
 <StackPanel x:Name="DashboardModules" Orientation="Horizontal" Spacing="6">
  <StackPanel.Resources>
   <Style TargetType="TextBlock"><Setter Property="Foreground" Value="{ThemeResource SystemControlForegroundBaseHighBrush}"/><Setter Property="FontFamily" Value="Segoe UI Variable"/><Setter Property="FontSize" Value="12"/><Setter Property="Typography.NumeralAlignment" Value="Tabular"/></Style>
   <Style TargetType="Button">
    <Setter Property="Width" Value="24"/><Setter Property="Height" Value="24"/><Setter Property="MinWidth" Value="0"/><Setter Property="MinHeight" Value="0"/><Setter Property="Padding" Value="0"/>
    <Setter Property="Foreground" Value="{ThemeResource SystemControlForegroundBaseHighBrush}"/>
    <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="Button"><Grid Background="Transparent">
     <VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates">
      <VisualState x:Name="Normal"/><VisualState x:Name="Disabled"><VisualState.Setters><Setter Target="content.Opacity" Value="0.30"/></VisualState.Setters></VisualState>
      <VisualState x:Name="PointerOver"><VisualState.Setters><Setter Target="content.Opacity" Value="0.75"/></VisualState.Setters></VisualState>
      <VisualState x:Name="Pressed"><VisualState.Setters><Setter Target="content.Opacity" Value="0.55"/></VisualState.Setters></VisualState>
     </VisualStateGroup></VisualStateManager.VisualStateGroups>
     <ContentPresenter x:Name="content" Content="{TemplateBinding Content}" HorizontalAlignment="Center" VerticalAlignment="Center"/>
    </Grid></ControlTemplate></Setter.Value></Setter>
   </Style>
  </StackPanel.Resources>
  <StackPanel x:Name="ClockGroup" Orientation="Horizontal" Width="84" Height="44">
   <Button x:Name="StartEntry" Width="24" Height="44" AutomationProperties.Name="开始菜单">
    <Path Width="14" Height="14" Stretch="Uniform" Fill="{ThemeResource SystemControlForegroundBaseHighBrush}" Data="M0,0 H6 V6 H0 Z M8,0 H14 V6 H8 Z M0,8 H6 V14 H0 Z M8,8 H14 V14 H8 Z"/>
   </Button>
   <Button x:Name="ClockCard" Width="60" Height="44" AutomationProperties.Name="时间与日期，打开日历和通知">
    <StackPanel x:Name="Clock"><TextBlock x:Name="ClockTime" FontSize="18" FontWeight="SemiBold" TextAlignment="Center"/>
     <TextBlock x:Name="ClockDate" FontSize="10" Opacity="0.85" TextAlignment="Center"/>
    </StackPanel>
   </Button>
  </StackPanel>
  <Grid x:Name="MetricsCard" Width="124" Height="44" Background="Transparent" AutomationProperties.Name="CPU 与内存">
   <Grid.RowDefinitions><RowDefinition Height="22"/><RowDefinition Height="22"/></Grid.RowDefinitions>
   <TextBlock x:Name="MetricsSummary" Text="CPU —  内存 —" FontSize="12" VerticalAlignment="Center"/>
   <TextBlock x:Name="MemoryUsage" Text="— / — GB" Grid.Row="1" FontSize="12" Opacity="0.85" VerticalAlignment="Center"/>
  </Grid>
  <Grid x:Name="PowerPlaceholder" Width="64" Height="44"/>
  <Grid x:Name="NetworkCard" Width="68" Height="44" Background="Transparent" AutomationProperties.Name="上传与下载速度">
   <Grid.RowDefinitions><RowDefinition Height="22"/><RowDefinition Height="22"/></Grid.RowDefinitions><Grid.ColumnDefinitions><ColumnDefinition Width="12"/><ColumnDefinition Width="4"/><ColumnDefinition Width="52"/></Grid.ColumnDefinitions>
   <TextBlock Text="↑" FontSize="14" Foreground="{ThemeResource LeanDockUploadBrush}" VerticalAlignment="Center"/>
   <TextBlock Text="↓" FontSize="14" Foreground="{ThemeResource LeanDockDownloadBrush}" Grid.Row="1" VerticalAlignment="Center"/>
   <Viewbox Grid.Column="2" MaxWidth="52" MaxHeight="22" Stretch="Uniform" StretchDirection="DownOnly" HorizontalAlignment="Right" VerticalAlignment="Center"><TextBlock x:Name="NetworkSend" Text="—" FontSize="12"/></Viewbox>
   <Viewbox Grid.Column="2" Grid.Row="1" MaxWidth="52" MaxHeight="22" Stretch="Uniform" StretchDirection="DownOnly" HorizontalAlignment="Right" VerticalAlignment="Center"><TextBlock x:Name="NetworkReceive" Text="—" FontSize="12"/></Viewbox>
  </Grid>
  <Grid x:Name="MediaGroup" Height="44" Visibility="Collapsed">
   <Grid x:Name="Media" Width="128" Height="44" Visibility="Collapsed"><Grid.RowDefinitions><RowDefinition Height="20"/><RowDefinition Height="24"/></Grid.RowDefinitions>
    <TextBlock x:Name="Title" Width="128" FontSize="12" TextTrimming="CharacterEllipsis" TextAlignment="Center" VerticalAlignment="Center"/>
    <StackPanel x:Name="MediaControls" Grid.Row="1" Orientation="Horizontal" Width="72" Height="24" HorizontalAlignment="Right" Margin="0,0,12,0">
     <Button x:Name="Previous" AutomationProperties.Name="上一首"><TextBlock FontFamily="Segoe Fluent Icons" FontSize="12" Text="&#xE892;"/></Button>
     <Button x:Name="Play" AutomationProperties.Name="播放或暂停"><TextBlock x:Name="PlayGlyph" FontFamily="Segoe Fluent Icons" FontSize="14" Text="&#xE768;"/></Button>
     <Button x:Name="Next" AutomationProperties.Name="下一首"><TextBlock FontFamily="Segoe Fluent Icons" FontSize="12" Text="&#xE893;"/></Button>
    </StackPanel>
   </Grid>
   <Button x:Name="MediaToggle" Width="24" Height="44" HorizontalAlignment="Left" VerticalAlignment="Top" AutomationProperties.Name="展开媒体控制"><TextBlock Text="&#xE8D6;" FontFamily="Segoe Fluent Icons" FontSize="16"/></Button>
  </Grid>
 </StackPanel>
</Border>)xaml";

static constexpr wchar_t DashboardPowerXaml[]=LR"xaml(
<Button xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
 x:Name="LeanDockPowerCard" Height="44" MinWidth="0" MinHeight="0" Padding="0"
 HorizontalAlignment="Center" VerticalAlignment="Center" AutomationProperties.Name="电池与快速设置">
 <Button.Template><ControlTemplate TargetType="Button">
  <Grid Background="Transparent"><VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates">
   <VisualState x:Name="Normal"/><VisualState x:Name="PointerOver"/><VisualState x:Name="Pressed"/>
  </VisualStateGroup></VisualStateManager.VisualStateGroups>
  <ContentPresenter Content="{TemplateBinding Content}" HorizontalAlignment="Center" VerticalAlignment="Center"/>
  </Grid>
 </ControlTemplate></Button.Template>
 <StackPanel x:Name="PowerContent" Width="64" HorizontalAlignment="Center">
  <StackPanel x:Name="PowerSummary" Orientation="Horizontal" Spacing="4" Height="24" HorizontalAlignment="Center">
   <Viewbox Width="24" Height="18" VerticalAlignment="Center">
    <Grid Width="32" Height="20" VerticalAlignment="Center">
     <Border x:Name="PowerBody" Width="28" Height="16" CornerRadius="3" BorderThickness="1" Padding="1" BorderBrush="{ThemeResource SystemControlForegroundBaseHighBrush}" HorizontalAlignment="Left" VerticalAlignment="Center">
      <Grid x:Name="PowerLevelHost">
       <Grid.ColumnDefinitions><ColumnDefinition Width="0*"/><ColumnDefinition Width="100*"/></Grid.ColumnDefinitions>
       <Rectangle x:Name="PowerFill" RadiusX="1" RadiusY="1" Fill="{ThemeResource SystemControlForegroundBaseHighBrush}" HorizontalAlignment="Stretch" VerticalAlignment="Stretch"/>
      </Grid>
     </Border>
     <Border Width="2" Height="6" CornerRadius="1" Background="{ThemeResource SystemControlForegroundBaseHighBrush}" HorizontalAlignment="Right" VerticalAlignment="Center"/>
     <Path x:Name="PowerBolt" Data="M 16,1 L 10,11 L 15,11 L 12,19 L 22,7 L 17,7 Z" Fill="{ThemeResource SystemControlForegroundBaseHighBrush}" Stroke="{ThemeResource SystemControlBackgroundChromeMediumLowBrush}" StrokeThickness="0.8" Visibility="Collapsed"/>
    </Grid>
   </Viewbox>
   <TextBlock x:Name="PowerText" Text="—" FontFamily="Segoe UI Variable" FontSize="14" FontWeight="SemiBold" VerticalAlignment="Center" Foreground="{ThemeResource SystemControlForegroundBaseHighBrush}"/>
  </StackPanel>
  <TextBlock x:Name="PowerStatus" Text="电源状态未知" Height="16" FontFamily="Segoe UI Variable" FontSize="9" Opacity="0.82" TextAlignment="Center" Foreground="{ThemeResource SystemControlForegroundBaseHighBrush}"/>
 </StackPanel>
</Button>)xaml";

// Reuse this exact layout path in the hidden XAML fixture. Four separate 24-DIP
// controls fit under the title; no extra full-height collapse column is needed.
static void ArrangeDashboardMedia(
    winrt::Windows::UI::Xaml::Controls::Grid const& group,
    winrt::Windows::UI::Xaml::Controls::Grid const& content,
    winrt::Windows::UI::Xaml::Controls::Button const& toggle,
    bool available,bool expanded) {
    using namespace winrt::Windows::UI::Xaml;
    expanded=available&&expanded;
    content.Visibility(expanded?Visibility::Visible:Visibility::Collapsed);
    group.Width(available?(expanded?LeanDockCards::Dashboard::MediaExpandedWidth:
        LeanDockCards::Dashboard::MediaToggleWidth):0);
    toggle.Height(expanded?24:44);
    toggle.Margin(expanded?Thickness{12,20,0,0}:Thickness{0,0,0,0});
}

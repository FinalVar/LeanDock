// Native XAML owns only the 72 x 46 information key. App and tray trees remain
// untouched. Theme resources remain live across light/dark/high-contrast changes.
// The date line carries a small bolt while the battery is charging; the pair
// stays centered. While a level is adjusted, the two lines show it instead.
static constexpr wchar_t AggregateXaml[]=LR"xaml(
<Border xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
 x:Name="LeanDockCard" Width="72" Height="46" Background="Transparent" HorizontalAlignment="Right" VerticalAlignment="Center">
 <Border.Resources><ResourceDictionary><ResourceDictionary.ThemeDictionaries>
  <ResourceDictionary x:Key="Default"><SolidColorBrush x:Key="RingTrack" Color="#29D0E9DD"/><SolidColorBrush x:Key="RingNormal" Color="#FF89C7B3"/><SolidColorBrush x:Key="RingLow" Color="#FFE1B25C"/><SolidColorBrush x:Key="RingCritical" Color="#FFED8E86"/></ResourceDictionary>
  <ResourceDictionary x:Key="Light"><SolidColorBrush x:Key="RingTrack" Color="#261E3E34"/><SolidColorBrush x:Key="RingNormal" Color="#FF368775"/><SolidColorBrush x:Key="RingLow" Color="#FFB88024"/><SolidColorBrush x:Key="RingCritical" Color="#FFC45A51"/></ResourceDictionary>
  <ResourceDictionary x:Key="HighContrast"><SolidColorBrush x:Key="RingTrack" Color="{ThemeResource SystemColorGrayTextColor}"/><SolidColorBrush x:Key="RingNormal" Color="{ThemeResource SystemColorWindowTextColor}"/><SolidColorBrush x:Key="RingLow" Color="{ThemeResource SystemColorWindowTextColor}"/><SolidColorBrush x:Key="RingCritical" Color="{ThemeResource SystemColorWindowTextColor}"/></ResourceDictionary>
 </ResourceDictionary.ThemeDictionaries></ResourceDictionary></Border.Resources>
 <Border.RenderTransform><TranslateTransform X="6"/></Border.RenderTransform>
 <Button x:Name="TimeKey" Width="72" Height="46" MinWidth="0" MinHeight="0" Padding="0" AutomationProperties.Name="时间、日期与电量，打开开始菜单">
  <Button.Template><ControlTemplate TargetType="Button"><Border x:Name="feedback" CornerRadius="23" Background="Transparent">
   <VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates">
    <VisualState x:Name="Normal"/><VisualState x:Name="PointerOver"><VisualState.Setters><Setter Target="feedback.Background" Value="{ThemeResource SystemControlHighlightListLowBrush}"/></VisualState.Setters></VisualState>
    <VisualState x:Name="Pressed"><VisualState.Setters><Setter Target="feedback.Opacity" Value="0.70"/></VisualState.Setters></VisualState>
   </VisualStateGroup></VisualStateManager.VisualStateGroups>
   <ContentPresenter Content="{TemplateBinding Content}" HorizontalAlignment="Center" VerticalAlignment="Center"/>
  </Border></ControlTemplate></Button.Template>
  <Grid Width="72" Height="46" IsHitTestVisible="False">
   <Rectangle x:Name="RingTrack" Width="68.8" Height="42.8" RadiusX="21.4" RadiusY="21.4" Stroke="{ThemeResource RingTrack}" StrokeThickness="1.4"/>
   <Path x:Name="RingNormal" Stroke="{ThemeResource RingNormal}" StrokeThickness="1.4" StrokeStartLineCap="Round" StrokeEndLineCap="Round" StrokeLineJoin="Round"/>
   <Path x:Name="RingLow" Stroke="{ThemeResource RingLow}" StrokeThickness="1.4" StrokeStartLineCap="Round" StrokeEndLineCap="Round" StrokeLineJoin="Round"/>
   <Path x:Name="RingCritical" Stroke="{ThemeResource RingCritical}" StrokeThickness="1.4" StrokeStartLineCap="Round" StrokeEndLineCap="Round" StrokeLineJoin="Round"/>
   <TextBlock x:Name="ClockTime" Height="22" Margin="0,5,0,0" VerticalAlignment="Top" TextAlignment="Center" FontFamily="Segoe UI" FontSize="18" FontWeight="SemiBold" Typography.NumeralAlignment="Tabular" Foreground="{ThemeResource SystemControlForegroundBaseHighBrush}"/>
   <StackPanel x:Name="DateLine" Orientation="Horizontal" Height="13" Margin="0,28,0,0" VerticalAlignment="Top" HorizontalAlignment="Center">
    <TextBlock x:Name="ClockDate" Height="13" TextAlignment="Center" FontFamily="Segoe UI" FontSize="11" Opacity="0.70" Typography.NumeralAlignment="Tabular" Foreground="{ThemeResource SystemControlForegroundBaseHighBrush}"/>
    <TextBlock x:Name="ChargeBolt" Visibility="Collapsed" Margin="2,0,0,0" VerticalAlignment="Center" FontFamily="Segoe Fluent Icons,Segoe MDL2 Assets" FontSize="9" Text="&#xE945;" Foreground="{ThemeResource RingNormal}"/>
   </StackPanel>
  </Grid>
 </Button>
</Border>)xaml";

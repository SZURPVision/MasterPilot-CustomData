namespace MasterPilot.Communicator.CustomData.Bindings;

internal partial struct mp_header_meta_t
{
    public bool IsEop
	{
		get => eop != 0;
		set => eop = (byte)(value ? 1 : 0);
	}
}